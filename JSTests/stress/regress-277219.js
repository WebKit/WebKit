function testScopeRestorationAfterExceptionInTry() {
  try {
    {
      let x = 1;
      var captureX = function() { print(x) };
      throw new Error("foo");
    }
  } catch {
    shouldBe(typeof x, "undefined");
  } finally {
    x
  }
}

function testScopeRestorationAfterExceptionInTry2() {
  try {
    for (let x of [1, 2, 3]) {
      var captureX = function() { print(x) };
      throw new Error("foo");
    }
  } catch {
    shouldBe(typeof x, "undefined");
  } finally {
    x
  }
}

function testScopeRestorationAfterExceptionInCatch() {
  try {
    throw new Error("foo");
  } catch (error) {
    {
      let x = 1;
      var captureX = function() { print(x) };
      throw new Error("foo");
    }
  } finally {
    shouldBe(typeof x, "undefined");
    x
  }
}

function testScopeRestorationAfterExceptionInCatch2() {
  try {
    throw new Error("foo");
  } catch {
    for (let x of [1, 2, 3]) {
      var captureX = function() { print(x) };
      throw new Error("foo");
    }
  } finally {
    shouldBe(typeof x, "undefined");
    x
  }
}

function shouldBe(actual, expected) {
  if (actual !== expected)
    throw new Error(`Bad value: ${actual}!`);
}

function shouldThrow(func, expectedMessage) {
  let errorThrown = false;
  try {
    func();
  } catch (error) {
    errorThrown = true;
    if (error.toString() !== expectedMessage)
      throw new Error(`Bad error: ${error}`);
  }
  if (!errorThrown)
    throw new Error(`Didn't throw!`);
}

for (var i = 0; i < 1e4; i++) {
  shouldThrow(testScopeRestorationAfterExceptionInTry, "ReferenceError: Can't find variable: x");
  shouldThrow(testScopeRestorationAfterExceptionInTry2, "ReferenceError: Can't find variable: x");
  shouldThrow(testScopeRestorationAfterExceptionInCatch, "ReferenceError: Can't find variable: x");
  shouldThrow(testScopeRestorationAfterExceptionInCatch2, "ReferenceError: Can't find variable: x");
}
