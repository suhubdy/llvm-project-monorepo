; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=i686 -mattr=cmov | FileCheck %s --check-prefixes=CHECK,X86
; RUN: llc < %s -mtriple=x86_64-linux | FileCheck %s --check-prefixes=CHECK,X64

declare  i4  @llvm.ssub.sat.i4   (i4,  i4)
declare  i32 @llvm.ssub.sat.i32  (i32, i32)
declare  i64 @llvm.ssub.sat.i64  (i64, i64)
declare  <4 x i32> @llvm.ssub.sat.v4i32(<4 x i32>, <4 x i32>)

define i32 @func(i32 %x, i32 %y) nounwind {
; X86-LABEL: func:
; X86:       # %bb.0:
; X86-NEXT:    pushl %esi
; X86-NEXT:    movl {{[0-9]+}}(%esp), %eax
; X86-NEXT:    movl {{[0-9]+}}(%esp), %edx
; X86-NEXT:    xorl %ecx, %ecx
; X86-NEXT:    movl %eax, %esi
; X86-NEXT:    subl %edx, %esi
; X86-NEXT:    setns %cl
; X86-NEXT:    addl $2147483647, %ecx # imm = 0x7FFFFFFF
; X86-NEXT:    subl %edx, %eax
; X86-NEXT:    cmovol %ecx, %eax
; X86-NEXT:    popl %esi
; X86-NEXT:    retl
;
; X64-LABEL: func:
; X64:       # %bb.0:
; X64-NEXT:    xorl %eax, %eax
; X64-NEXT:    movl %edi, %ecx
; X64-NEXT:    subl %esi, %ecx
; X64-NEXT:    setns %al
; X64-NEXT:    addl $2147483647, %eax # imm = 0x7FFFFFFF
; X64-NEXT:    subl %esi, %edi
; X64-NEXT:    cmovnol %edi, %eax
; X64-NEXT:    retq
  %tmp = call i32 @llvm.ssub.sat.i32(i32 %x, i32 %y);
  ret i32 %tmp;
}

define i64 @func2(i64 %x, i64 %y) nounwind {
; X86-LABEL: func2:
; X86:       # %bb.0:
; X86-NEXT:    pushl %ebp
; X86-NEXT:    pushl %ebx
; X86-NEXT:    pushl %edi
; X86-NEXT:    pushl %esi
; X86-NEXT:    movl {{[0-9]+}}(%esp), %esi
; X86-NEXT:    movl {{[0-9]+}}(%esp), %edi
; X86-NEXT:    movl {{[0-9]+}}(%esp), %ebx
; X86-NEXT:    subl {{[0-9]+}}(%esp), %edi
; X86-NEXT:    movl %ebx, %ebp
; X86-NEXT:    sbbl %esi, %ebp
; X86-NEXT:    movl %ebp, %eax
; X86-NEXT:    sarl $31, %eax
; X86-NEXT:    xorl %ecx, %ecx
; X86-NEXT:    testl %ebp, %ebp
; X86-NEXT:    setns %cl
; X86-NEXT:    movl %ecx, %edx
; X86-NEXT:    addl $2147483647, %edx # imm = 0x7FFFFFFF
; X86-NEXT:    testl %ebx, %ebx
; X86-NEXT:    setns %bl
; X86-NEXT:    cmpb %cl, %bl
; X86-NEXT:    setne %cl
; X86-NEXT:    testl %esi, %esi
; X86-NEXT:    setns %ch
; X86-NEXT:    cmpb %ch, %bl
; X86-NEXT:    setne %ch
; X86-NEXT:    testb %cl, %ch
; X86-NEXT:    cmovel %ebp, %edx
; X86-NEXT:    cmovel %edi, %eax
; X86-NEXT:    popl %esi
; X86-NEXT:    popl %edi
; X86-NEXT:    popl %ebx
; X86-NEXT:    popl %ebp
; X86-NEXT:    retl
;
; X64-LABEL: func2:
; X64:       # %bb.0:
; X64-NEXT:    xorl %ecx, %ecx
; X64-NEXT:    movq %rdi, %rax
; X64-NEXT:    subq %rsi, %rax
; X64-NEXT:    setns %cl
; X64-NEXT:    movabsq $9223372036854775807, %rax # imm = 0x7FFFFFFFFFFFFFFF
; X64-NEXT:    addq %rcx, %rax
; X64-NEXT:    subq %rsi, %rdi
; X64-NEXT:    cmovnoq %rdi, %rax
; X64-NEXT:    retq
  %tmp = call i64 @llvm.ssub.sat.i64(i64 %x, i64 %y);
  ret i64 %tmp;
}

define i4 @func3(i4 %x, i4 %y) nounwind {
; X86-LABEL: func3:
; X86:       # %bb.0:
; X86-NEXT:    movb {{[0-9]+}}(%esp), %al
; X86-NEXT:    movb {{[0-9]+}}(%esp), %dl
; X86-NEXT:    shlb $4, %dl
; X86-NEXT:    shlb $4, %al
; X86-NEXT:    movl %eax, %ecx
; X86-NEXT:    subb %dl, %cl
; X86-NEXT:    setns %cl
; X86-NEXT:    subb %dl, %al
; X86-NEXT:    jno .LBB2_2
; X86-NEXT:  # %bb.1:
; X86-NEXT:    addb $127, %cl
; X86-NEXT:    movl %ecx, %eax
; X86-NEXT:  .LBB2_2:
; X86-NEXT:    sarb $4, %al
; X86-NEXT:    retl
;
; X64-LABEL: func3:
; X64:       # %bb.0:
; X64-NEXT:    movl %edi, %eax
; X64-NEXT:    shlb $4, %sil
; X64-NEXT:    shlb $4, %al
; X64-NEXT:    movl %eax, %ecx
; X64-NEXT:    subb %sil, %cl
; X64-NEXT:    setns %cl
; X64-NEXT:    subb %sil, %al
; X64-NEXT:    jno .LBB2_2
; X64-NEXT:  # %bb.1:
; X64-NEXT:    addb $127, %cl
; X64-NEXT:    movl %ecx, %eax
; X64-NEXT:  .LBB2_2:
; X64-NEXT:    sarb $4, %al
; X64-NEXT:    # kill: def $al killed $al killed $eax
; X64-NEXT:    retq
  %tmp = call i4 @llvm.ssub.sat.i4(i4 %x, i4 %y);
  ret i4 %tmp;
}

define <4 x i32> @vec(<4 x i32> %x, <4 x i32> %y) nounwind {
; X86-LABEL: vec:
; X86:       # %bb.0:
; X86-NEXT:    pushl %ebp
; X86-NEXT:    pushl %ebx
; X86-NEXT:    pushl %edi
; X86-NEXT:    pushl %esi
; X86-NEXT:    movl {{[0-9]+}}(%esp), %ecx
; X86-NEXT:    movl {{[0-9]+}}(%esp), %edx
; X86-NEXT:    xorl %eax, %eax
; X86-NEXT:    movl %ecx, %esi
; X86-NEXT:    subl %edx, %esi
; X86-NEXT:    setns %al
; X86-NEXT:    addl $2147483647, %eax # imm = 0x7FFFFFFF
; X86-NEXT:    subl %edx, %ecx
; X86-NEXT:    movl {{[0-9]+}}(%esp), %edx
; X86-NEXT:    cmovol %eax, %ecx
; X86-NEXT:    movl {{[0-9]+}}(%esp), %esi
; X86-NEXT:    xorl %eax, %eax
; X86-NEXT:    movl %edx, %edi
; X86-NEXT:    subl %esi, %edi
; X86-NEXT:    setns %al
; X86-NEXT:    addl $2147483647, %eax # imm = 0x7FFFFFFF
; X86-NEXT:    subl %esi, %edx
; X86-NEXT:    movl {{[0-9]+}}(%esp), %esi
; X86-NEXT:    cmovol %eax, %edx
; X86-NEXT:    movl {{[0-9]+}}(%esp), %edi
; X86-NEXT:    xorl %eax, %eax
; X86-NEXT:    movl %esi, %ebx
; X86-NEXT:    subl %edi, %ebx
; X86-NEXT:    setns %al
; X86-NEXT:    addl $2147483647, %eax # imm = 0x7FFFFFFF
; X86-NEXT:    subl %edi, %esi
; X86-NEXT:    movl {{[0-9]+}}(%esp), %edi
; X86-NEXT:    cmovol %eax, %esi
; X86-NEXT:    movl {{[0-9]+}}(%esp), %eax
; X86-NEXT:    xorl %ebx, %ebx
; X86-NEXT:    movl %edi, %ebp
; X86-NEXT:    subl %eax, %ebp
; X86-NEXT:    setns %bl
; X86-NEXT:    addl $2147483647, %ebx # imm = 0x7FFFFFFF
; X86-NEXT:    subl %eax, %edi
; X86-NEXT:    movl {{[0-9]+}}(%esp), %eax
; X86-NEXT:    cmovol %ebx, %edi
; X86-NEXT:    movl %ecx, 12(%eax)
; X86-NEXT:    movl %edx, 8(%eax)
; X86-NEXT:    movl %esi, 4(%eax)
; X86-NEXT:    movl %edi, (%eax)
; X86-NEXT:    popl %esi
; X86-NEXT:    popl %edi
; X86-NEXT:    popl %ebx
; X86-NEXT:    popl %ebp
; X86-NEXT:    retl $4
;
; X64-LABEL: vec:
; X64:       # %bb.0:
; X64-NEXT:    movdqa %xmm0, %xmm2
; X64-NEXT:    pxor %xmm3, %xmm3
; X64-NEXT:    pxor %xmm0, %xmm0
; X64-NEXT:    pcmpgtd %xmm1, %xmm0
; X64-NEXT:    pcmpeqd %xmm4, %xmm4
; X64-NEXT:    pxor %xmm4, %xmm0
; X64-NEXT:    pxor %xmm5, %xmm5
; X64-NEXT:    pcmpgtd %xmm2, %xmm5
; X64-NEXT:    pxor %xmm4, %xmm5
; X64-NEXT:    pcmpeqd %xmm5, %xmm0
; X64-NEXT:    psubd %xmm1, %xmm2
; X64-NEXT:    pcmpgtd %xmm2, %xmm3
; X64-NEXT:    movdqa %xmm3, %xmm1
; X64-NEXT:    pxor %xmm4, %xmm1
; X64-NEXT:    pcmpeqd %xmm5, %xmm1
; X64-NEXT:    pxor %xmm4, %xmm1
; X64-NEXT:    pandn %xmm1, %xmm0
; X64-NEXT:    movdqa %xmm3, %xmm1
; X64-NEXT:    pandn {{.*}}(%rip), %xmm1
; X64-NEXT:    psrld $1, %xmm3
; X64-NEXT:    por %xmm1, %xmm3
; X64-NEXT:    pand %xmm0, %xmm3
; X64-NEXT:    pandn %xmm2, %xmm0
; X64-NEXT:    por %xmm3, %xmm0
; X64-NEXT:    retq
  %tmp = call <4 x i32> @llvm.ssub.sat.v4i32(<4 x i32> %x, <4 x i32> %y);
  ret <4 x i32> %tmp;
}
