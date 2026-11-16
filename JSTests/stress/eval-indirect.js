
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType, assertionFn) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name} but got ${error.name}`);

    assertionFn(error);
}

shouldBe((0, eval)("() => { return 1; }")(), 1);

shouldThrow(
  () => { (0, eval)("..") },
  SyntaxError,
  (err) => {
      shouldBe(String(err), `SyntaxError: Unexpected token '.'`);
  });

// NOTE: more iterations (like 20000) will trigger a GC/OOM issue on 32-bit devices.
for (var i = 0; i < 1000; ++i) {
    let f = (0, eval)("() => { return 1; }");
    shouldBe(f(), 1);
}
