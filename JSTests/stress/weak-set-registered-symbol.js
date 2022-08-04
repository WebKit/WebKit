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

var s1 = Symbol("A");
var s2 = Symbol.for("B");

function test() {
    var set = new WeakSet();
    shouldBe(set.has(s1), false);
    shouldBe(set.has(s2), false);

    set.add(s1);
    shouldThrow(() => {
        set.add(s2);
    }, `TypeError: WeakSet values must be objects or non-registered symbols`);

    shouldBe(set.has(s1), true);
    shouldBe(set.has(s2), false);

    shouldBe(set.delete(s1), true);
    shouldBe(set.has(s1), false);

    shouldBe(set.delete(s2), false);
    shouldBe(set.has(s2), false);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
