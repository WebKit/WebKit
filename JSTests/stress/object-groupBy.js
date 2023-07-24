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

shouldBe(Object.groupBy.length, 2);
shouldBe(Object.groupBy.name, "groupBy");


// Array

shouldBeObject(Object.groupBy([undefined], (x) => x === undefined ? "a" : "b"), {"a": [undefined]});
shouldBeObject(Object.groupBy([undefined], (x) => x === undefined), {"true": [undefined]});

shouldBeObject(Object.groupBy(new Array(4), (x) => x === undefined ? "a" : "b"), {"a": [undefined, undefined, undefined, undefined]});
shouldBeObject(Object.groupBy(new Array(4), (x) => x === undefined), {"true": [undefined, undefined, undefined, undefined]});

shouldBeObject(Object.groupBy([0, 1, 2, 3], (x) => !(x & 1) ? "a" : "b"), {"a": [0, 2], "b": [1, 3]});
shouldBeObject(Object.groupBy([0, 1, 2, 3], (x) => !(x & 1)), {"true": [0, 2], "false": [1, 3]});

shouldBeObject(Object.groupBy([0, 1, 2, 3], (x, i) => i >= 2 ? "a" : "b"), {"b": [0, 1], "a": [2, 3]});
shouldBeObject(Object.groupBy([0, 1, 2, 3], (x, i) => i >= 2), {"false": [0, 1], "true": [2, 3]});

shouldBeObject(Object.groupBy(mixPartialAndFast, (x, i) => i >= 2 ? "a" : "b"), {"b": [0, 1], "a": [2, undefined, undefined, sparseArrayLength - 1]});
shouldBeObject(Object.groupBy(mixPartialAndFast, (x, i) => i >= 2), {"false": [0, 1], "true": [2, undefined, undefined, sparseArrayLength - 1]});


// Set

shouldBeObject(Object.groupBy(new Set([undefined]), (x) => x === undefined ? "a" : "b"), {"a": [undefined]});
shouldBeObject(Object.groupBy(new Set([undefined]), (x) => x === undefined), {"true": [undefined]});

shouldBeObject(Object.groupBy(new Set(new Array(4)), (x) => x === undefined ? "a" : "b"), {"a": [undefined]});
shouldBeObject(Object.groupBy(new Set(new Array(4)), (x) => x === undefined), {"true": [undefined]});

shouldBeObject(Object.groupBy(new Set([0, 1, 2, 3]), (x) => !(x & 1) ? "a" : "b"), {"a": [0, 2], "b": [1, 3]});
shouldBeObject(Object.groupBy(new Set([0, 1, 2, 3]), (x) => !(x & 1)), {"true": [0, 2], "false": [1, 3]});

shouldBeObject(Object.groupBy(new Set([0, 1, 2, 3]), (x, i) => i >= 2 ? "a" : "b"), {"b": [0, 1], "a": [2, 3]});
shouldBeObject(Object.groupBy(new Set([0, 1, 2, 3]), (x, i) => i >= 2), {"false": [0, 1], "true": [2, 3]});

shouldBeObject(Object.groupBy(new Set(mixPartialAndFast), (x, i) => i >= 2 ? "a" : "b"), {"b": [0, 1], "a": [2, undefined, sparseArrayLength - 1]});
shouldBeObject(Object.groupBy(new Set(mixPartialAndFast), (x, i) => i >= 2), {"false": [0, 1], "true": [2, undefined, sparseArrayLength - 1]});


// Extra callback parameters

shouldBeObject(Object.groupBy([0, 1, 2, 3], (i, j, k, l, m) => m = !m), {"true": [0, 1, 2, 3]});


// Special keys

shouldBeObject(Object.groupBy([0, 1, 2, 3], (x) => "constructor").constructor, [0, 1, 2, 3]);

shouldBeObject(Object.groupBy([0, 1, 2, 3], (x) => "prototype").prototype, [0, 1, 2, 3]);
shouldBeObject(Object.groupBy([0, 1, 2, 3], (x) => "__proto__").__proto__, [0, 1, 2, 3]);

shouldBeObject(Object.groupBy([0, 1, 2, 3], (x) => -0)[0], [0, 1, 2, 3]);
shouldBeObject(Object.groupBy([0, 1, 2, 3], (x) => 0)[0], [0, 1, 2, 3]);

let objectWithToStringCounter = {
    counter: 0,
    toString() { return this.counter++; },
};
shouldBeObject(Object.groupBy([0, 1, 2, 3], (x) => objectWithToStringCounter), {"0": [0], "1": [1], "2": [2], "3": [3]});
shouldBe(objectWithToStringCounter.counter, 4);

try {
    shouldBeObject(Object.groupBy([0, 1, 2, 3], (x) => objectWithToStringThatThrows), {});
    notReached();
} catch (e) {
    shouldBe(e.message, "should not reach here");
}

shouldBeObject(Object.groupBy([0, 1, 2, 3], (x) => objectWithValueOfThatThrows)["[object Object]"], [0, 1, 2, 3]);

shouldBeObject(Object.groupBy([0, 1, 2, 3], (x) => symbol)[symbol], [0, 1, 2, 3]);


// Invalid parameters

try {
    shouldBeObject(Object.groupBy(null, () => {}), {});
    notReached();
} catch (e) {
    shouldBe(String(e), "TypeError: Object.groupBy requires that the first argument must be an object");
}

try {
    shouldBeObject(Object.groupBy({}, null), {});
    notReached();
} catch (e) {
    shouldBe(String(e), "TypeError: Object.groupBy requires that the second argument must be a function");
}
