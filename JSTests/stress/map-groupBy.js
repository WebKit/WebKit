//@ requireOptions("--useArrayGroupMethod=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

function shouldBeObject(actual, expected) {
    function replacer(key, value) {
        if (value === undefined)
            return "%undefined%";
        return value;
    }

    let actualJSON = JSON.stringify(actual, replacer).replaceAll("\"%undefined%\"", "undefined");
    let expectedJSON = JSON.stringify(expected, replacer).replaceAll("\"%undefined%\"", "undefined");
    shouldBe(actualJSON, expectedJSON);
}

function shouldBeMap(a, b) {
    shouldBeObject(Array.from(a), b);
}

function notReached() {
    throw new Error("should not reach here");
}


// Helper variables

let symbol = Symbol("symbol");

let sparseArrayLength = 6;
let mixPartialAndFast = new Array(sparseArrayLength);
mixPartialAndFast[sparseArrayLength - 1] = sparseArrayLength - 1;
for(let i = 0; i < 3; ++i)
    mixPartialAndFast[i] = i;

let objectWithToStringThatThrows = {
    toString: notReached,
};

let objectWithValueOfThatThrows = {
    valueOf: notReached,
};


// Basic

shouldBe(Map.groupBy.length, 2);
shouldBe(Map.groupBy.name, "groupBy");


// Array

shouldBeMap(Map.groupBy([undefined], (x) => x === undefined ? "a" : "b"), [["a", [undefined]]]);
shouldBeMap(Map.groupBy([undefined], (x) => x === undefined), [[true, [undefined]]]);

shouldBeMap(Map.groupBy(new Array(4), (x) => x === undefined ? "a" : "b"), [["a", [undefined, undefined, undefined, undefined]]]);
shouldBeMap(Map.groupBy(new Array(4), (x) => x === undefined), [[true, [undefined, undefined, undefined, undefined]]]);

shouldBeMap(Map.groupBy([0, 1, 2, 3], (x) => !(x & 1) ? "a" : "b"), [["a", [0, 2]], ["b", [1, 3]]]);
shouldBeMap(Map.groupBy([0, 1, 2, 3], (x) => !(x & 1)), [[true, [0, 2]], [false, [1, 3]]]);

shouldBeMap(Map.groupBy([0, 1, 2, 3], (x, i) => i >= 2 ? "a" : "b"), [["b", [0, 1]], ["a", [2, 3]]]);
shouldBeMap(Map.groupBy([0, 1, 2, 3], (x, i) => i >= 2), [[false, [0, 1]], [true, [2, 3]]]);

shouldBeMap(Map.groupBy(mixPartialAndFast, (x, i) => i >= 2 ? "a" : "b"), [["b", [0, 1]], ["a", [2, undefined, undefined, sparseArrayLength - 1]]]);
shouldBeMap(Map.groupBy(mixPartialAndFast, (x, i) => i >= 2), [[false, [0, 1]], [true, [2, undefined, undefined, sparseArrayLength - 1]]]);


// Set

shouldBeMap(Map.groupBy(new Set([undefined]), (x) => x === undefined ? "a" : "b"), [["a", [undefined]]]);
shouldBeMap(Map.groupBy(new Set([undefined]), (x) => x === undefined), [[true, [undefined]]]);

shouldBeMap(Map.groupBy(new Set(new Array(4)), (x) => x === undefined ? "a" : "b"), [["a", [undefined]]]);
shouldBeMap(Map.groupBy(new Set(new Array(4)), (x) => x === undefined), [[true, [undefined]]]);

shouldBeMap(Map.groupBy(new Set([0, 1, 2, 3]), (x) => !(x & 1) ? "a" : "b"), [["a", [0, 2]], ["b", [1, 3]]]);
shouldBeMap(Map.groupBy(new Set([0, 1, 2, 3]), (x) => !(x & 1)), [[true, [0, 2]], [false, [1, 3]]]);

shouldBeMap(Map.groupBy(new Set([0, 1, 2, 3]), (x, i) => i >= 2 ? "a" : "b"), [["b", [0, 1]], ["a", [2, 3]]]);
shouldBeMap(Map.groupBy(new Set([0, 1, 2, 3]), (x, i) => i >= 2), [[false, [0, 1]], [true, [2, 3]]]);

shouldBeMap(Map.groupBy(new Set(mixPartialAndFast), (x, i) => i >= 2 ? "a" : "b"), [["b", [0, 1]], ["a", [2, undefined, sparseArrayLength - 1]]]);
shouldBeMap(Map.groupBy(new Set(mixPartialAndFast), (x, i) => i >= 2), [[false, [0, 1]], [true, [2, undefined, sparseArrayLength - 1]]]);


// Extra callback parameters

shouldBeMap(Map.groupBy([0, 1, 2, 3], (i, j, k, l, m) => m = !m), [[true, [0, 1, 2, 3]]]);


// Special keys

shouldBeObject(Map.groupBy([0, 1, 2, 3], (x) => "constructor").get("constructor"), [0, 1, 2, 3]);

shouldBeObject(Map.groupBy([0, 1, 2, 3], (x) => "prototype").get("prototype"), [0, 1, 2, 3]);
shouldBeObject(Map.groupBy([0, 1, 2, 3], (x) => "__proto__").get("__proto__"), [0, 1, 2, 3]);

shouldBeObject(Map.groupBy([0, 1, 2, 3], (x) => -0).get(0), [0, 1, 2, 3]);
shouldBeObject(Map.groupBy([0, 1, 2, 3], (x) => 0).get(0), [0, 1, 2, 3]);

let objectWithToStringCounter = {
    counter: 0,
    toString() { return this.counter++; },
};
shouldBeObject(Map.groupBy([0, 1, 2, 3], (x) => objectWithToStringCounter).get(objectWithToStringCounter), [0, 1, 2, 3]);
shouldBe(objectWithToStringCounter.counter, 0);

shouldBeObject(Map.groupBy([0, 1, 2, 3], (x) => objectWithToStringThatThrows).get(objectWithToStringThatThrows), [0, 1, 2, 3]);

shouldBeObject(Map.groupBy([0, 1, 2, 3], (x) => objectWithValueOfThatThrows).get(objectWithValueOfThatThrows), [0, 1, 2, 3]);

shouldBeObject(Map.groupBy([0, 1, 2, 3], (x) => symbol).get(symbol), [0, 1, 2, 3]);


// Invalid parameters

try {
    shouldBeMap(Map.groupBy(null, () => {}), []);
    notReached();
} catch (e) {
    shouldBe(String(e), "TypeError: Map.groupBy requires that the first argument must be an object");
}

try {
    shouldBeMap(Map.groupBy({}, null), []);
    notReached();
} catch (e) {
    shouldBe(String(e), "TypeError: Map.groupBy requires that the second argument must be a function");
}
