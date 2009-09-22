description('Test for (foo in somethingWhichThrows) to catch ASSERT');

function throwNullException() {
  throw null;
}

function throwUndefinedException() {
  throw undefined;
}

function throwStringException() {
  throw "PASSED"
}

function test(func) {
  for (var foo in func()) {
    testFailed("Shoud not be reached");
  }
}

shouldBeUndefined("test(throwUndefinedException)");
shouldBeUndefined("test(throwNullException)");
shouldThrow("test(throwStringException)");

var successfullyParsed = true;
