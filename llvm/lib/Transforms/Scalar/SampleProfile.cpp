//===- SampleProfile.cpp - Incorporate sample profiles into the IR --------===//
//
//                      The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the SampleProfileLoader transformation. This pass
// reads a profile file generated by a sampling profiler (e.g. Linux Perf -
// http://perf.wiki.kernel.org/) and generates IR metadata to reflect the
// profile information in the given profile.
//
// This pass generates branch weight annotations on the IR:
//
// - prof: Represents branch weights. This annotation is added to branches
//      to indicate the weights of each edge coming out of the branch.
//      The weight of each edge is the weight of the target block for
//      that edge. The weight of a block B is computed as the maximum
//      number of samples found in B.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "sample-profile"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"

using namespace llvm;

// Command line option to specify the file to read samples from. This is
// mainly used for debugging.
static cl::opt<std::string> SampleProfileFile(
    "sample-profile-file", cl::init(""), cl::value_desc("filename"),
    cl::desc("Profile file loaded by -sample-profile"), cl::Hidden);

namespace {

typedef DenseMap<uint32_t, uint32_t> BodySampleMap;
typedef DenseMap<BasicBlock *, uint32_t> BlockWeightMap;

/// \brief Representation of the runtime profile for a function.
///
/// This data structure contains the runtime profile for a given
/// function. It contains the total number of samples collected
/// in the function and a map of samples collected in every statement.
class SampleFunctionProfile {
public:
  SampleFunctionProfile() : TotalSamples(0), TotalHeadSamples(0) {}

  bool emitAnnotations(Function &F);
  uint32_t getInstWeight(Instruction &I, unsigned FirstLineno,
                         BodySampleMap &BodySamples);
  uint32_t computeBlockWeight(BasicBlock *B, unsigned FirstLineno,
                              BodySampleMap &BodySamples);
  void addTotalSamples(unsigned Num) { TotalSamples += Num; }
  void addHeadSamples(unsigned Num) { TotalHeadSamples += Num; }
  void addBodySamples(unsigned LineOffset, unsigned Num) {
    BodySamples[LineOffset] += Num;
  }
  void print(raw_ostream &OS);

protected:
  /// \brief Total number of samples collected inside this function.
  ///
  /// Samples are cumulative, they include all the samples collected
  /// inside this function and all its inlined callees.
  unsigned TotalSamples;

  // \brief Total number of samples collected at the head of the function.
  unsigned TotalHeadSamples;

  /// \brief Map line offsets to collected samples.
  ///
  /// Each entry in this map contains the number of samples
  /// collected at the corresponding line offset. All line locations
  /// are an offset from the start of the function.
  BodySampleMap BodySamples;

  /// \brief Map basic blocks to their computed weights.
  ///
  /// The weight of a basic block is defined to be the maximum
  /// of all the instruction weights in that block.
  BlockWeightMap BlockWeights;
};

/// \brief Sample-based profile reader.
///
/// Each profile contains sample counts for all the functions
/// executed. Inside each function, statements are annotated with the
/// collected samples on all the instructions associated with that
/// statement.
///
/// For this to produce meaningful data, the program needs to be
/// compiled with some debug information (at minimum, line numbers:
/// -gline-tables-only). Otherwise, it will be impossible to match IR
/// instructions to the line numbers collected by the profiler.
///
/// From the profile file, we are interested in collecting the
/// following information:
///
/// * A list of functions included in the profile (mangled names).
///
/// * For each function F:
///   1. The total number of samples collected in F.
///
///   2. The samples collected at each line in F. To provide some
///      protection against source code shuffling, line numbers should
///      be relative to the start of the function.
class SampleModuleProfile {
public:
  SampleModuleProfile(StringRef F) : Profiles(0), Filename(F) {}

  void dump();
  void loadText();
  void loadNative() { llvm_unreachable("not implemented"); }
  void printFunctionProfile(raw_ostream &OS, StringRef FName);
  void dumpFunctionProfile(StringRef FName);
  SampleFunctionProfile &getProfile(const Function &F) {
    return Profiles[F.getName()];
  }

protected:
  /// \brief Map every function to its associated profile.
  ///
  /// The profile of every function executed at runtime is collected
  /// in the structure SampleFunctionProfile. This maps function objects
  /// to their corresponding profiles.
  StringMap<SampleFunctionProfile> Profiles;

  /// \brief Path name to the file holding the profile data.
  ///
  /// The format of this file is defined by each profiler
  /// independently. If possible, the profiler should have a text
  /// version of the profile format to be used in constructing test
  /// cases and debugging.
  StringRef Filename;
};

/// \brief Loader class for text-based profiles.
///
/// This class defines a simple interface to read text files containing
/// profiles. It keeps track of line number information and location of
/// the file pointer. Users of this class are responsible for actually
/// parsing the lines returned by the readLine function.
///
/// TODO - This does not really belong here. It is a generic text file
/// reader. It should be moved to the Support library and made more general.
class ExternalProfileTextLoader {
public:
  ExternalProfileTextLoader(StringRef F) : Filename(F) {
    error_code EC;
    EC = MemoryBuffer::getFile(Filename, Buffer);
    if (EC)
      report_fatal_error("Could not open profile file " + Filename + ": " +
                         EC.message());
    FP = Buffer->getBufferStart();
    Lineno = 0;
  }

  /// \brief Read a line from the mapped file.
  StringRef readLine() {
    size_t Length = 0;
    const char *start = FP;
    while (FP != Buffer->getBufferEnd() && *FP != '\n') {
      Length++;
      FP++;
    }
    if (FP != Buffer->getBufferEnd())
      FP++;
    Lineno++;
    return StringRef(start, Length);
  }

  /// \brief Return true, if we've reached EOF.
  bool atEOF() const { return FP == Buffer->getBufferEnd(); }

  /// \brief Report a parse error message and stop compilation.
  void reportParseError(Twine Msg) const {
    report_fatal_error(Filename + ":" + Twine(Lineno) + ": " + Msg + "\n");
  }

private:
  /// \brief Memory buffer holding the text file.
  OwningPtr<MemoryBuffer> Buffer;

  /// \brief Current position into the memory buffer.
  const char *FP;

  /// \brief Current line number.
  int64_t Lineno;

  /// \brief Path name where to the profile file.
  StringRef Filename;
};

/// \brief Sample profile pass.
///
/// This pass reads profile data from the file specified by
/// -sample-profile-file and annotates every affected function with the
/// profile information found in that file.
class SampleProfileLoader : public FunctionPass {
public:
  // Class identification, replacement for typeinfo
  static char ID;

  SampleProfileLoader(StringRef Name = SampleProfileFile)
      : FunctionPass(ID), Profiler(0), Filename(Name) {
    initializeSampleProfileLoaderPass(*PassRegistry::getPassRegistry());
  }

  virtual bool doInitialization(Module &M);

  void dump() { Profiler->dump(); }

  virtual const char *getPassName() const { return "Sample profile pass"; }

  virtual bool runOnFunction(Function &F);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesCFG();
  }

protected:
  /// \brief Profile reader object.
  OwningPtr<SampleModuleProfile> Profiler;

  /// \brief Name of the profile file to load.
  StringRef Filename;
};
}

/// \brief Print this function profile on stream \p OS.
///
/// \param OS Stream to emit the output to.
void SampleFunctionProfile::print(raw_ostream &OS) {
  OS << TotalSamples << ", " << TotalHeadSamples << ", " << BodySamples.size()
     << " sampled lines\n";
  for (BodySampleMap::const_iterator SI = BodySamples.begin(),
                                     SE = BodySamples.end();
       SI != SE; ++SI)
    OS << "\tline offset: " << SI->first
       << ", number of samples: " << SI->second << "\n";
  OS << "\n";
}

/// \brief Print the function profile for \p FName on stream \p OS.
///
/// \param OS Stream to emit the output to.
/// \param FName Name of the function to print.
void SampleModuleProfile::printFunctionProfile(raw_ostream &OS,
                                               StringRef FName) {
  OS << "Function: " << FName << ":\n";
  Profiles[FName].print(OS);
}

/// \brief Dump the function profile for \p FName.
///
/// \param FName Name of the function to print.
void SampleModuleProfile::dumpFunctionProfile(StringRef FName) {
  printFunctionProfile(dbgs(), FName);
}

/// \brief Dump all the function profiles found.
void SampleModuleProfile::dump() {
  for (StringMap<SampleFunctionProfile>::const_iterator I = Profiles.begin(),
                                                        E = Profiles.end();
       I != E; ++I)
    dumpFunctionProfile(I->getKey());
}

/// \brief Load samples from a text file.
///
/// The file is divided in two segments:
///
/// Symbol table (represented with the string "symbol table")
///    Number of symbols in the table
///    symbol 1
///    symbol 2
///    ...
///    symbol N
///
/// Function body profiles
///    function1:total_samples:total_head_samples:number_of_locations
///    location_offset_1: number_of_samples
///    location_offset_2: number_of_samples
///    ...
///    location_offset_N: number_of_samples
///
/// Function names must be mangled in order for the profile loader to
/// match them in the current translation unit.
///
/// Since this is a flat profile, a function that shows up more than
/// once gets all its samples aggregated across all its instances.
/// TODO - flat profiles are too imprecise to provide good optimization
/// opportunities. Convert them to context-sensitive profile.
///
/// This textual representation is useful to generate unit tests and
/// for debugging purposes, but it should not be used to generate
/// profiles for large programs, as the representation is extremely
/// inefficient.
void SampleModuleProfile::loadText() {
  ExternalProfileTextLoader Loader(Filename);

  // Read the symbol table.
  StringRef Line = Loader.readLine();
  if (Line != "symbol table")
    Loader.reportParseError("Expected 'symbol table', found " + Line);
  int NumSymbols;
  Line = Loader.readLine();
  if (Line.getAsInteger(10, NumSymbols))
    Loader.reportParseError("Expected a number, found " + Line);
  for (int I = 0; I < NumSymbols; I++)
    Profiles[Loader.readLine()] = SampleFunctionProfile();

  // Read the profile of each function. Since each function may be
  // mentioned more than once, and we are collecting flat profiles,
  // accumulate samples as we parse them.
  Regex HeadRE("^([^:]+):([0-9]+):([0-9]+):([0-9]+)$");
  Regex LineSample("^([0-9]+): ([0-9]+)$");
  while (!Loader.atEOF()) {
    SmallVector<StringRef, 4> Matches;
    Line = Loader.readLine();
    if (!HeadRE.match(Line, &Matches))
      Loader.reportParseError("Expected 'mangled_name:NUM:NUM:NUM', found " +
                              Line);
    assert(Matches.size() == 5);
    StringRef FName = Matches[1];
    unsigned NumSamples, NumHeadSamples, NumSampledLines;
    Matches[2].getAsInteger(10, NumSamples);
    Matches[3].getAsInteger(10, NumHeadSamples);
    Matches[4].getAsInteger(10, NumSampledLines);
    SampleFunctionProfile &FProfile = Profiles[FName];
    FProfile.addTotalSamples(NumSamples);
    FProfile.addHeadSamples(NumHeadSamples);
    unsigned I;
    for (I = 0; I < NumSampledLines && !Loader.atEOF(); I++) {
      Line = Loader.readLine();
      if (!LineSample.match(Line, &Matches))
        Loader.reportParseError("Expected 'NUM: NUM', found " + Line);
      assert(Matches.size() == 3);
      unsigned LineOffset, NumSamples;
      Matches[1].getAsInteger(10, LineOffset);
      Matches[2].getAsInteger(10, NumSamples);
      FProfile.addBodySamples(LineOffset, NumSamples);
    }

    if (I < NumSampledLines)
      Loader.reportParseError("Unexpected end of file");
  }
}

char SampleProfileLoader::ID = 0;
INITIALIZE_PASS(SampleProfileLoader, "sample-profile", "Sample Profile loader",
                false, false)

bool SampleProfileLoader::doInitialization(Module &M) {
  Profiler.reset(new SampleModuleProfile(Filename));
  Profiler->loadText();
  return true;
}

FunctionPass *llvm::createSampleProfileLoaderPass() {
  return new SampleProfileLoader(SampleProfileFile);
}

FunctionPass *llvm::createSampleProfileLoaderPass(StringRef Name) {
  return new SampleProfileLoader(Name);
}

/// \brief Get the weight for an instruction.
///
/// The "weight" of an instruction \p Inst is the number of samples
/// collected on that instruction at runtime. To retrieve it, we
/// need to compute the line number of \p Inst relative to the start of its
/// function. We use \p FirstLineno to compute the offset. We then
/// look up the samples collected for \p Inst using \p BodySamples.
///
/// \param Inst Instruction to query.
/// \param FirstLineno Line number of the first instruction in the function.
/// \param BodySamples Map of relative source line locations to samples.
///
/// \returns The profiled weight of I.
uint32_t SampleFunctionProfile::getInstWeight(Instruction &Inst,
                                              unsigned FirstLineno,
                                              BodySampleMap &BodySamples) {
  unsigned LOffset = Inst.getDebugLoc().getLine() - FirstLineno + 1;
  return BodySamples.lookup(LOffset);
}

/// \brief Compute the weight of a basic block.
///
/// The weight of basic block \p B is the maximum weight of all the
/// instructions in B.
///
/// \param B The basic block to query.
/// \param FirstLineno The line number for the first line in the
///     function holding B.
/// \param BodySamples The map containing all the samples collected in that
///     function.
///
/// \returns The computed weight of B.
uint32_t SampleFunctionProfile::computeBlockWeight(BasicBlock *B,
                                                   unsigned FirstLineno,
                                                   BodySampleMap &BodySamples) {
  // If we've computed B's weight before, return it.
  std::pair<BlockWeightMap::iterator, bool> Entry =
      BlockWeights.insert(std::make_pair(B, 0));
  if (!Entry.second)
    return Entry.first->second;

  // Otherwise, compute and cache B's weight.
  uint32_t Weight = 0;
  for (BasicBlock::iterator I = B->begin(), E = B->end(); I != E; ++I) {
    uint32_t InstWeight = getInstWeight(*I, FirstLineno, BodySamples);
    if (InstWeight > Weight)
      Weight = InstWeight;
  }
  Entry.first->second = Weight;
  return Weight;
}

/// \brief Generate branch weight metadata for all branches in \p F.
///
/// For every branch instruction B in \p F, we compute the weight of the
/// target block for each of the edges out of B. This is the weight
/// that we associate with that branch.
///
/// TODO - This weight assignment will most likely be wrong if the
/// target branch has more than two predecessors. This needs to be done
/// using some form of flow propagation.
///
/// Once all the branch weights are computed, we emit the MD_prof
/// metadata on B using the computed values.
///
/// \param F The function to query.
bool SampleFunctionProfile::emitAnnotations(Function &F) {
  bool Changed = false;
  unsigned FirstLineno = inst_begin(F)->getDebugLoc().getLine();
  MDBuilder MDB(F.getContext());

  // Clear the block weights cache.
  BlockWeights.clear();

  // When we find a branch instruction: For each edge E out of the branch,
  // the weight of E is the weight of the target block.
  for (Function::iterator I = F.begin(), E = F.end(); I != E; ++I) {
    BasicBlock *B = I;
    TerminatorInst *TI = B->getTerminator();
    if (TI->getNumSuccessors() == 1)
      continue;
    if (!isa<BranchInst>(TI) && !isa<SwitchInst>(TI))
      continue;

    SmallVector<uint32_t, 4> Weights;
    unsigned NSuccs = TI->getNumSuccessors();
    for (unsigned I = 0; I < NSuccs; ++I) {
      BasicBlock *Succ = TI->getSuccessor(I);
      uint32_t Weight = computeBlockWeight(Succ, FirstLineno, BodySamples);
      Weights.push_back(Weight);
    }

    TI->setMetadata(llvm::LLVMContext::MD_prof,
                    MDB.createBranchWeights(Weights));
    Changed = true;
  }

  return Changed;
}

bool SampleProfileLoader::runOnFunction(Function &F) {
  return Profiler->getProfile(F).emitAnnotations(F);
}
