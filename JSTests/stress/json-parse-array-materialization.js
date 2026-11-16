function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldThrow(source) {
    var threw = false;
    try {
        JSON.parse(source);
    } catch (error) {
        threw = true;
    }
    shouldBe(threw, true);
}

// An array is allocated at its final length and indexing type, so every element type combination
// has to land in the same indexing shape a growing butterfly would have reached, and the values
// have to survive a garbage collection that happens while the elements are still being collected.
var cases = [
    ["[]", []],
    ["[0]", [0]],
    ["[1,2,3]", [1, 2, 3]],
    ["[-2147483648,2147483647]", [-2147483648, 2147483647]],
    ["[1.5,2.5]", [1.5, 2.5]],
    ["[1,2.5]", [1, 2.5]],
    ["[2.5,1]", [2.5, 1]],
    ["[1e400,1]", [Infinity, 1]],
    ["[null]", [null]],
    ["[true,false]", [true, false]],
    ["[\"a\",\"b\"]", ["a", "b"]],
    ["[1,\"a\"]", [1, "a"]],
    ["[\"a\",1]", ["a", 1]],
    ["[[1],[2]]", [[1], [2]]],
    ["[{\"a\":1},{\"a\":2}]", [{ a: 1 }, { a: 2 }]],
    ["[1,[2,[3,[4]]]]", [1, [2, [3, [4]]]]],
];

for (var i = 0; i < 1e3; ++i) {
    for (var [source, expected] of cases) {
        var parsed = JSON.parse(source);
        shouldBe(JSON.stringify(parsed), JSON.stringify(expected));
        shouldBe(parsed.length, expected.length);
        shouldBe(Array.isArray(parsed), true);
    }
}

// -0 must stay -0 rather than being flattened to 0 by the double indexing shape.
shouldBe(JSON.parse("[-0]")[0], -0);
shouldBe(JSON.parse("[-0,1.5]")[0], -0);
shouldBe(JSON.parse("[1.5,-0]")[1], -0);

// Lengths that cross the growth thresholds of the butterfly the old path grew element by element.
for (var length of [1, 2, 3, 4, 5, 8, 9, 16, 17, 100, 1024, 100000]) {
    var ints = JSON.parse("[" + Array.from({ length }, (_, i) => i).join(",") + "]");
    shouldBe(ints.length, length);
    shouldBe(ints[0], 0);
    shouldBe(ints[length - 1], length - 1);

    var doubles = JSON.parse("[" + Array.from({ length }, (_, i) => i + 0.5).join(",") + "]");
    shouldBe(doubles.length, length);
    shouldBe(doubles[length - 1], length - 1 + 0.5);

    var strings = JSON.parse("[" + Array.from({ length }, (_, i) => '"' + i + '"').join(",") + "]");
    shouldBe(strings.length, length);
    shouldBe(strings[length - 1], String(length - 1));
}

// A malformed array must still report the same error, and must not leave collected elements behind.
shouldThrow("[1,]");
shouldThrow("[,1]");
shouldThrow("[1 2]");
shouldThrow("[");
shouldThrow("[1");
shouldThrow("[}");
shouldThrow("[[1],]");
shouldThrow("[1,[2,]]");
shouldThrow('[{"a":1},]');
for (var i = 0; i < 1e3; ++i)
    shouldThrow("[1,2,3,]");
shouldBe(JSON.stringify(JSON.parse("[1,2,3]")), "[1,2,3]");

// Nesting past the recursive parser's stack limit hands the value to the iterative parser.
var depth = 20000;
var deep = JSON.parse("[".repeat(depth) + "1" + "]".repeat(depth));
var levels = 0;
while (Array.isArray(deep)) {
    shouldBe(deep.length, 1);
    deep = deep[0];
    ++levels;
}
shouldBe(levels, depth);
shouldBe(deep, 1);

// An array index as an object key stays an indexed property, and a duplicate key keeps the last
// value, both of which the fast property path has to decline.
var indexed = JSON.parse('{"0":1,"b":2}');
shouldBe(JSON.stringify(Object.keys(indexed)), '["0","b"]');
shouldBe(indexed[0], 1);
shouldBe(indexed.b, 2);

var duplicate = JSON.parse('{"a":1,"a":2}');
shouldBe(JSON.stringify(Object.keys(duplicate)), '["a"]');
shouldBe(duplicate.a, 2);

// Objects wide enough to need out-of-line storage exercise the butterfly growth check.
for (var count of [1, 5, 6, 7, 8, 20, 64, 65, 200]) {
    var source = "{" + Array.from({ length: count }, (_, i) => '"p' + i + '":' + i).join(",") + "}";
    var wide = JSON.parse(source);
    shouldBe(Object.keys(wide).length, count);
    shouldBe(wide["p0"], 0);
    shouldBe(wide["p" + (count - 1)], count - 1);
}

// A reviver goes through the general parser, which must agree with the fast one.
shouldBe(JSON.stringify(JSON.parse("[1,2,3]", (key, value) => typeof value === "number" ? value * 2 : value)), "[2,4,6]");
shouldBe(JSON.stringify(JSON.parse('{"a":[1,2]}', (key, value) => value)), '{"a":[1,2]}');

// An indexed accessor on Array.prototype makes the global object have a bad time, after which arrays
// are allocated with array storage instead of a contiguous vector.
Object.defineProperty(Array.prototype, "1", { get() { return "bad"; }, configurable: true });
for (var [source, expected] of cases) {
    var parsed = JSON.parse(source);
    shouldBe(JSON.stringify(parsed), JSON.stringify(expected));
    shouldBe(parsed.length, expected.length);
}
var afterBadTime = JSON.parse("[" + Array.from({ length: 1000 }, (_, i) => i).join(",") + "]");
shouldBe(afterBadTime.length, 1000);
shouldBe(afterBadTime[999], 999);
shouldBe(JSON.parse("[7,8]")[1], 8);
