function foo() {
  delete arguments[2**32-1];
}
foo();
