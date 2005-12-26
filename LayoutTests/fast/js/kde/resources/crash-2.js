// infinite recursion 2
function foo() {
   foo();
}

try {
  foo();
} catch (e) {
  debug("OK. Caught an exception");
}
successfullyParsed = true
