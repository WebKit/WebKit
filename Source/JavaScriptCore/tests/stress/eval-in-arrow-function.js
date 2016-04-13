function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var global = this;
for (var i = 0; i < 100; ++i) {
    (() => {
        // |this| should reference to the global one.
        shouldBe(eval("this"), global);
    })();
}

for (var i = 0; i < 100; ++i) {
    var THIS = {};
    (function test() {
        // |this| should reference to the function's one.
        shouldBe(eval("this"), THIS);
    }).call(THIS);
}
