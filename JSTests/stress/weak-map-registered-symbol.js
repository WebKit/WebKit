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
    var map = new WeakMap();
    shouldBe(map.get(s1), undefined);
    shouldBe(map.get(s2), undefined);

    shouldBe(map.has(s1), false);
    shouldBe(map.has(s2), false);

    map.set(s1, 1);
    shouldThrow(() => {
        map.set(s2, 2);
    }, `TypeError: WeakMap keys must be objects or non-registered symbols`);

    shouldBe(map.get(s1), 1);
    shouldBe(map.get(s2), undefined);

    shouldBe(map.has(s1), true);
    shouldBe(map.has(s2), false);

    shouldBe(map.delete(s1), true);
    shouldBe(map.has(s1), false);

    shouldBe(map.delete(s2), false);
    shouldBe(map.has(s2), false);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
