// RUN: %clang_cc1 -std=c++11 %s -Wunused -verify


struct X {
  X(const X&) = delete; // expected-note 2{{explicitly marked deleted}}
  X(X&);
};

void test_capture(X x) {
  [x] { }(); // okay: non-const copy ctor

  [x] {
    [x] { // expected-error{{call to deleted constructor of 'X'}}
    }();
  }();

  [x] {
    [&x] {
      [x] { // expected-error{{call to deleted constructor of 'const X'}}
      }();
    }();
  }();

  int a; 
  [=]{ 
    [&] { 
      int &x = a;  // expected-error{{binding value of type 'const int' to reference of type 'int' drops qualifiers}}
      int &x2 = a;  // expected-error{{binding value of type 'const int' to reference of type 'int' drops qualifiers}}
    }(); 
  }(); 

  [=]{ 
    [&a] { 
      [&] { 
        int &x = a;  // expected-error{{binding value of type 'const int' to reference of type 'int' drops qualifiers}}
        int &x2 = a;  // expected-error{{binding value of type 'const int' to reference of type 'int' drops qualifiers}}
      }();
    }(); 
  }(); 
}
