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

Array.prototype.__test = 42;

let symbol = Symbol("symbol");

let sparseArrayLength = 6;
let mixPartialAndFast = new Array(sparseArrayLength);
mixPartialAndFast[sparseArrayLength - 1] = sparseArrayLength - 1;
for(let i = 0; i < 3; ++i)
    mixPartialAndFast[i] = i;

function toObject(array) {
    let result = {};
    result.length = array.length;
    for (let i in array)
        result[i] = array[i];
    result.groupToMap = Array.prototype.groupToMap;
    return result;
}

function reverseInsertionOrder(array) {
    let obj = toObject(array);
    let props = [];
    for (let i in obj)
        props.push(i);
    let result = {};
    for (let i = props.length - 1; i >= 0; i--)
        result[props[i]] = obj[props[i]];
    result.groupToMap = Array.prototype.groupToMap;
    return result;
}

let objectWithToStringThatThrows = {
    toString: notReached,
};

let objectWithValueOfThatThrows = {
    valueOf: notReached,
};


// Basic

shouldBe(Array.prototype.groupToMap.length, 1);
shouldBe(Array.prototype.groupToMap.name, "groupToMap");

shouldBeMap([undefined].groupToMap((x) => x === undefined ? "a" : "b"), [["a", [undefined]]]);
shouldBeMap([undefined].groupToMap((x) => x === undefined), [[true, [undefined]]]);

shouldBeMap((new Array(4)).groupToMap((x) => x === undefined ? "a" : "b"), [["a", [undefined, undefined, undefined, undefined]]]);
shouldBeMap((new Array(4)).groupToMap((x) => x === undefined), [[true, [undefined, undefined, undefined, undefined]]]);

shouldBeMap([0, 1, 2, 3].groupToMap((x) => !(x & 1) ? "a" : "b"), [["a", [0, 2]], ["b", [1, 3]]]);
shouldBeMap([0, 1, 2, 3].groupToMap((x) => !(x & 1)), [[true, [0, 2]], [false, [1, 3]]]);

shouldBeMap([0, 1, 2, 3].groupToMap((x, i) => i >= 2 ? "a" : "b"), [["b", [0, 1]], ["a", [2, 3]]]);
shouldBeMap([0, 1, 2, 3].groupToMap((x, i) => i >= 2), [[false, [0, 1]], [true, [2, 3]]]);

shouldBeMap(mixPartialAndFast.groupToMap((x, i) => i >= 2 ? "a" : "b"), [["b", [0, 1]], ["a", [2, undefined, undefined, sparseArrayLength - 1]]]);
shouldBeMap(mixPartialAndFast.groupToMap((x, i) => i >= 2), [[false, [0, 1]], [true, [2, undefined, undefined, sparseArrayLength - 1]]]);


// Generic Object

shouldBeMap(toObject([undefined]).groupToMap((x) => x === undefined ? "a" : "b"), [["a", [undefined]]]);
shouldBeMap(toObject([undefined]).groupToMap((x) => x === undefined), [[true, [undefined]]]);

shouldBeMap(toObject(new Array(4)).groupToMap((x) => x === undefined ? "a" : "b"), [["a", [undefined, undefined, undefined, undefined]]]);
shouldBeMap(toObject(new Array(4)).groupToMap((x) => x === undefined), [[true, [undefined, undefined, undefined, undefined]]]);

shouldBeMap(toObject([0, 1, 2, 3]).groupToMap((x) => !(x & 1) ? "a" : "b"), [["a", [0, 2]], ["b", [1, 3]]]);
shouldBeMap(toObject([0, 1, 2, 3]).groupToMap((x) => !(x & 1)), [[true, [0, 2]], [false, [1, 3]]]);

shouldBeMap(toObject([0, 1, 2, 3]).groupToMap((x, i) => i >= 2 ? "a" : "b"), [["b", [0, 1]], ["a", [2, 3]]]);
shouldBeMap(toObject([0, 1, 2, 3]).groupToMap((x, i) => i >= 2), [[false, [0, 1]], [true, [2, 3]]]);

shouldBeMap(toObject(mixPartialAndFast).groupToMap((x, i) => i >= 2 ? "a" : "b"), [["b", [0, 1]], ["a", [2, undefined, undefined, sparseArrayLength - 1]]]);
shouldBeMap(toObject(mixPartialAndFast).groupToMap((x, i) => i >= 2), [[false, [0, 1]], [true, [2, undefined, undefined, sparseArrayLength - 1]]]);


// Array-like object with invalid lengths

shouldBeMap(Array.prototype.groupToMap.call({ 0: 0, 1: 1, 2: 2, 3: 3, length: 0 }, notReached), []);
shouldBeMap(Array.prototype.groupToMap.call({ 0: 0, 1: 1, 2: 2, 3: 3, length: -0 }, notReached), []);
shouldBeMap(Array.prototype.groupToMap.call({ 0: 0, 1: 1, 2: 2, 3: 3, length: -4 }, notReached), []);


// Reversed generic Object

shouldBeMap(reverseInsertionOrder([undefined]).groupToMap((x) => x === undefined ? "a" : "b"), [["a", [undefined]]]);
shouldBeMap(reverseInsertionOrder([undefined]).groupToMap((x) => x === undefined), [[true, [undefined]]]);

shouldBeMap(reverseInsertionOrder(new Array(4)).groupToMap((x) => x === undefined ? "a" : "b"), [["a", [undefined, undefined, undefined, undefined]]]);
shouldBeMap(reverseInsertionOrder(new Array(4)).groupToMap((x) => x === undefined), [[true, [undefined, undefined, undefined, undefined]]]);

shouldBeMap(reverseInsertionOrder([0, 1, 2, 3]).groupToMap((x) => !(x & 1) ? "a" : "b"), [["a", [0, 2]], ["b", [1, 3]]]);
shouldBeMap(reverseInsertionOrder([0, 1, 2, 3]).groupToMap((x) => !(x & 1)), [[true, [0, 2]], [false, [1, 3]]]);

shouldBeMap(reverseInsertionOrder([0, 1, 2, 3]).groupToMap((x, i) => i >= 2 ? "a" : "b"), [["b", [0, 1]], ["a", [2, 3]]]);
shouldBeMap(reverseInsertionOrder([0, 1, 2, 3]).groupToMap((x, i) => i >= 2), [[false, [0, 1]], [true, [2, 3]]]);

shouldBeMap(reverseInsertionOrder(mixPartialAndFast).groupToMap((x, i) => i >= 2 ? "a" : "b"), [["b", [0, 1]], ["a", [2, undefined, undefined, sparseArrayLength - 1]]]);
shouldBeMap(reverseInsertionOrder(mixPartialAndFast).groupToMap((x, i) => i >= 2), [[false, [0, 1]], [true, [2, undefined, undefined, sparseArrayLength - 1]]]);


// Extra callback parameters

shouldBeMap([0, 1, 2, 3].groupToMap((i, j, k, l, m) => m = !m), [[true, [0, 1, 2, 3]]]);


// Special keys

shouldBeObject([0, 1, 2, 3].groupToMap((x) => "constructor").get("constructor"), [0, 1, 2, 3]);

shouldBeObject([0, 1, 2, 3].groupToMap((x) => "prototype").get("prototype"), [0, 1, 2, 3]);
shouldBeObject([0, 1, 2, 3].groupToMap((x) => "__proto__").get("__proto__"), [0, 1, 2, 3]);

shouldBeObject([0, 1, 2, 3].groupToMap((x) => -0).get(0), [0, 1, 2, 3]);
shouldBeObject([0, 1, 2, 3].groupToMap((x) => 0).get(0), [0, 1, 2, 3]);

let objectWithToStringCounter = {
    counter: 0,
    toString() { return this.counter++; },
};
shouldBeObject([0, 1, 2, 3].groupToMap((x) => objectWithToStringCounter).get(objectWithToStringCounter), [0, 1, 2, 3]);
shouldBe(objectWithToStringCounter.counter, 0);

shouldBeObject([0, 1, 2, 3].groupToMap((x) => objectWithToStringThatThrows).get(objectWithToStringThatThrows), [0, 1, 2, 3]);

shouldBeObject([0, 1, 2, 3].groupToMap((x) => objectWithValueOfThatThrows).get(objectWithValueOfThatThrows), [0, 1, 2, 3]);

shouldBeObject([0, 1, 2, 3].groupToMap((x) => symbol).get(symbol), [0, 1, 2, 3]);
