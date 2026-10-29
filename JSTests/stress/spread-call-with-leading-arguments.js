function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)} expected: ${String(expected)}`);
}

function describe() {
    "use strict";
    let result = String(this) + "|" + arguments.length + ":";
    for (let i = 0; i < arguments.length; ++i)
        result += String(arguments[i]) + ",";
    return result;
}
noInline(describe);

function describeInlined(a, b, c) {
    "use strict";
    return String(this) + "|" + arguments.length + ":" + String(a) + "," + String(b) + "," + String(c);
}

class Point {
    constructor(x, y, z) {
        this.result = arguments.length + ":" + String(x) + "," + String(y) + "," + String(z);
    }
}

function call(first, rest) { return describe(first, ...rest); }
noInline(call);
function callInlined(first, rest) { return describeInlined(first, ...rest); }
noInline(callInlined);
function callWithThis(thisValue, rest) { return describe.call(thisValue, ...rest); }
noInline(callWithThis);
function callWithThisInlined(thisValue, rest) { return describeInlined.call(thisValue, ...rest); }
noInline(callWithThisInlined);
function callWithOnlySpread(rest) { return describe.call(...rest, ...rest); }
noInline(callWithOnlySpread);
function construct(first, rest) { return new Point(first, ...rest); }
noInline(construct);

const rests = [[], [1], [1.5, 2.5], ["a", {}, null], [1, , 3]];
const expectedRests = ["", "1,", "1.5,2.5,", "a,[object Object],null,", "1,undefined,3,"];

for (let i = 0; i < testLoopCount; ++i) {
    const rest = rests[i % rests.length];
    const expectedRest = expectedRests[i % rests.length];

    shouldBe(call("first", rest), `undefined|${rest.length + 1}:first,${expectedRest}`);
    shouldBe(callInlined("first", rest), `undefined|${rest.length + 1}:first,${String(rest[0])},${String(rest[1])}`);
    shouldBe(callWithThis("this", rest), `this|${rest.length}:${expectedRest}`);
    shouldBe(callWithThisInlined("this", rest), `this|${rest.length}:${String(rest[0])},${String(rest[1])},${String(rest[2])}`);
    shouldBe(callWithOnlySpread(rest), rest.length ? `${String(rest[0])}|${rest.length * 2 - 1}:${expectedRest.slice(String(rest[0]).length + 1)}${expectedRest}` : "undefined|0:");
    shouldBe(construct("first", rest).result, `${rest.length + 1}:first,${String(rest[0])},${String(rest[1])}`);
}
