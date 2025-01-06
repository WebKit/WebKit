//@ requireOptions("--useMapGetOrInsert=1")

function assert(a, text) {
    if (!a)
        throw new Error(`Failed assertion: ${text}`);
}

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

let map = new Map;

shouldBe(map.getOrInsertComputed("a", function(key) {
    assert(key === "a", "should provide key");
    return 1;
}), 1);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1]]`);
shouldBe(map.getOrInsertComputed("a", function(key) {
    assert(false, "not reached");
    return 2;
}), 1);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1]]`);
shouldBe(map.getOrInsertComputed("a", function(key) {
    assert(false, "not reached");
    return 3;
}), 1);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1]]`);

shouldBe(map.getOrInsertComputed("b", function(key) {
    assert(key === "b", "should provide key");
    return 3;
}), 3);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1],["b",3]]`);
shouldBe(map.getOrInsertComputed("b", function(key) {
    assert(false, "not reached");
    return 2;
}), 3);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1],["b",3]]`);
shouldBe(map.getOrInsertComputed("b", function(key) {
    assert(false, "not reached");
    return 1;
}), 3);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1],["b",3]]`);

for (let invalid of [null, undefined, true, "test", 42, [], {}]) {
    try {
        map.getOrInsertComputed(42, invalid);
        assert(false, "should throw");
    } catch (e) {
        assert(e instanceof TypeError, "should throw");
    }
}
