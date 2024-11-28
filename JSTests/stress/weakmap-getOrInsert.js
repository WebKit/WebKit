//@ requireOptions("--useMapGetOrInsert=1")

function assert(a, text) {
    if (!a)
        throw new Error(`Failed assertion: ${text}`);
}

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

let object1 = {};
let object2 = {};

let weakmap = new WeakMap;

shouldBe(weakmap.getOrInsert(object1, 1), 1);
shouldBe(weakmap.getOrInsert(object1, 2), 1);
shouldBe(weakmap.getOrInsert(object1, 3), 1);

shouldBe(weakmap.getOrInsert(object2, 3), 3);
shouldBe(weakmap.getOrInsert(object2, 2), 3);
shouldBe(weakmap.getOrInsert(object2, 1), 3);

for (let invalid of [null, undefined, true, "test", 42, Symbol.for("symbol")]) {
    try {
        weakmap.getOrInsert(invalid, 42);
        assert(false, "should throw");
    } catch (e) {
        assert(e instanceof TypeError, "should throw");
    }
}
