//@ requireOptions("--useArrayGroupByMethod=1")

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
    result.groupBy = Array.prototype.groupBy;
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
    result.groupBy = Array.prototype.groupBy;
    return result;
}

let objectWithToStringThatThrows = {
    toString: notReached,
};

let objectWithValueOfThatThrows = {
    valueOf: notReached,
};


// Basic

shouldBe(Array.prototype.groupBy.length, 1);
shouldBe(Array.prototype.groupBy.name, "groupBy");

shouldBeObject([undefined].groupBy((x) => x === undefined ? "a" : "b"), {"a": [undefined]});
shouldBeObject([undefined].groupBy((x) => x === undefined), {"true": [undefined]});

shouldBeObject((new Array(4)).groupBy((x) => x === undefined ? "a" : "b"), {"a": [undefined, undefined, undefined, undefined]});
shouldBeObject((new Array(4)).groupBy((x) => x === undefined), {"true": [undefined, undefined, undefined, undefined]});

shouldBeObject([0, 1, 2, 3].groupBy((x) => !(x & 1) ? "a" : "b"), {"a": [0, 2], "b": [1, 3]});
shouldBeObject([0, 1, 2, 3].groupBy((x) => !(x & 1)), {"true": [0, 2], "false": [1, 3]});

shouldBeObject([0, 1, 2, 3].groupBy((x, i) => i >= 2 ? "a" : "b"), {"b": [0, 1], "a": [2, 3]});
shouldBeObject([0, 1, 2, 3].groupBy((x, i) => i >= 2), {"false": [0, 1], "true": [2, 3]});

shouldBeObject(mixPartialAndFast.groupBy((x, i) => i >= 2 ? "a" : "b"), {"b": [0, 1], "a": [2, undefined, undefined, sparseArrayLength - 1]});
shouldBeObject(mixPartialAndFast.groupBy((x, i) => i >= 2), {"false": [0, 1], "true": [2, undefined, undefined, sparseArrayLength - 1]});


// Generic Object

shouldBeObject(toObject([undefined]).groupBy((x) => x === undefined ? "a" : "b"), {"a": [undefined]});
shouldBeObject(toObject([undefined]).groupBy((x) => x === undefined), {"true": [undefined]});

shouldBeObject(toObject(new Array(4)).groupBy((x) => x === undefined ? "a" : "b"), {"a": [undefined, undefined, undefined, undefined]});
shouldBeObject(toObject(new Array(4)).groupBy((x) => x === undefined), {"true": [undefined, undefined, undefined, undefined]});

shouldBeObject(toObject([0, 1, 2, 3]).groupBy((x) => !(x & 1) ? "a" : "b"), {"a": [0, 2], "b": [1, 3]});
shouldBeObject(toObject([0, 1, 2, 3]).groupBy((x) => !(x & 1)), {"true": [0, 2], "false": [1, 3]});

shouldBeObject(toObject([0, 1, 2, 3]).groupBy((x, i) => i >= 2 ? "a" : "b"), {"b": [0, 1], "a": [2, 3]});
shouldBeObject(toObject([0, 1, 2, 3]).groupBy((x, i) => i >= 2), {"false": [0, 1], "true": [2, 3]});

shouldBeObject(toObject(mixPartialAndFast).groupBy((x, i) => i >= 2 ? "a" : "b"), {"b": [0, 1], "a": [2, undefined, undefined, sparseArrayLength - 1]});
shouldBeObject(toObject(mixPartialAndFast).groupBy((x, i) => i >= 2), {"false": [0, 1], "true": [2, undefined, undefined, sparseArrayLength - 1]});


// Array-like object with invalid lengths

shouldBeObject(Array.prototype.groupBy.call({ 0: 0, 1: 1, 2: 2, 3: 3, length: 0 }, notReached), {});
shouldBeObject(Array.prototype.groupBy.call({ 0: 0, 1: 1, 2: 2, 3: 3, length: -0 }, notReached), {});
shouldBeObject(Array.prototype.groupBy.call({ 0: 0, 1: 1, 2: 2, 3: 3, length: -4 }, notReached), {});


// Reversed generic Object

shouldBeObject(reverseInsertionOrder([undefined]).groupBy((x) => x === undefined ? "a" : "b"), {"a": [undefined]});
shouldBeObject(reverseInsertionOrder([undefined]).groupBy((x) => x === undefined), {"true": [undefined]});

shouldBeObject(reverseInsertionOrder(new Array(4)).groupBy((x) => x === undefined ? "a" : "b"), {"a": [undefined, undefined, undefined, undefined]});
shouldBeObject(reverseInsertionOrder(new Array(4)).groupBy((x) => x === undefined), {"true": [undefined, undefined, undefined, undefined]});

shouldBeObject(reverseInsertionOrder([0, 1, 2, 3]).groupBy((x) => !(x & 1) ? "a" : "b"), {"a": [0, 2], "b": [1, 3]});
shouldBeObject(reverseInsertionOrder([0, 1, 2, 3]).groupBy((x) => !(x & 1)), {"true": [0, 2], "false": [1, 3]});

shouldBeObject(reverseInsertionOrder([0, 1, 2, 3]).groupBy((x, i) => i >= 2 ? "a" : "b"), {"b": [0, 1], "a": [2, 3]});
shouldBeObject(reverseInsertionOrder([0, 1, 2, 3]).groupBy((x, i) => i >= 2), {"false": [0, 1], "true": [2, 3]});

shouldBeObject(reverseInsertionOrder(mixPartialAndFast).groupBy((x, i) => i >= 2 ? "a" : "b"), {"b": [0, 1], "a": [2, undefined, undefined, sparseArrayLength - 1]});
shouldBeObject(reverseInsertionOrder(mixPartialAndFast).groupBy((x, i) => i >= 2), {"false": [0, 1], "true": [2, undefined, undefined, sparseArrayLength - 1]});


// Extra callback parameters

shouldBeObject([0, 1, 2, 3].groupBy((i, j, k, l, m) => m = !m), {"true": [0, 1, 2, 3]});


// Special keys

shouldBeObject([0, 1, 2, 3].groupBy((x) => "constructor").constructor, [0, 1, 2, 3]);

shouldBeObject([0, 1, 2, 3].groupBy((x) => "prototype").prototype, [0, 1, 2, 3]);
shouldBeObject([0, 1, 2, 3].groupBy((x) => "__proto__").__proto__, [0, 1, 2, 3]);

shouldBeObject([0, 1, 2, 3].groupBy((x) => -0)[0], [0, 1, 2, 3]);
shouldBeObject([0, 1, 2, 3].groupBy((x) => 0)[0], [0, 1, 2, 3]);

let objectWithToStringCounter = {
    counter: 0,
    toString() { return this.counter++; },
};
shouldBeObject([0, 1, 2, 3].groupBy((x) => objectWithToStringCounter), {"0": [0], "1": [1], "2": [2], "3": [3]});
shouldBe(objectWithToStringCounter.counter, 4);

try {
    shouldBeObject([0, 1, 2, 3].groupBy((x) => objectWithToStringThatThrows), {});
    notReached();
} catch (e) {
    shouldBe(e.message, "should not reach here");
}

shouldBeObject([0, 1, 2, 3].groupBy((x) => objectWithValueOfThatThrows)["[object Object]"], [0, 1, 2, 3]);

shouldBeObject([0, 1, 2, 3].groupBy((x) => symbol)[symbol], [0, 1, 2, 3]);
