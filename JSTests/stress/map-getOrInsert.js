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

shouldBe(map.getOrInsert("a", 1), 1);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1]]`);
shouldBe(map.getOrInsert("a", 2), 1);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1]]`);
shouldBe(map.getOrInsert("a", 3), 1);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1]]`);

shouldBe(map.getOrInsert("b", 3), 3);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1],["b",3]]`);
shouldBe(map.getOrInsert("b", 2), 3);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1],["b",3]]`);
shouldBe(map.getOrInsert("b", 1), 3);
shouldBe(JSON.stringify(Array.from(map)), `[["a",1],["b",3]]`);
