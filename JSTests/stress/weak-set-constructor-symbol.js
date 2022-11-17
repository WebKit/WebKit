function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
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

function test() {
    var symbol = Symbol("Hello");
    var set = new WeakSet([
        symbol
    ]);
    shouldBe(set.has(symbol), true);
    shouldThrow(() => {
        var registered = Symbol.for("Hello");
        new WeakSet([ registered ]);
    }, `TypeError: WeakSet values must be objects or non-registered symbols`);
}

for (var i = 0; i < 1e4; ++i)
    test();
