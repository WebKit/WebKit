function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual));
}

function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

function toCodePoints(string) {
    var result = [];
    for (var codePoint of string) {
        result.push(codePoint.codePointAt(0));
    }
    return result;
}

shouldBe(String.fromCodePoint(), "");
shouldBe(String.fromCodePoint(0), "\0");
shouldBe(String.fromCodePoint(0, 0), "\0\0");

var tests = [
    "",
    "Hello",
    "Cocoa",
    "Cappuccino Cocoa",
    "日本語",
    "マルチバイト",
    "吉野屋",
    "𠮷野家",  // Contain a surrogate pair.
];

for (var test of tests) {
    shouldBe(String.fromCodePoint.apply(String, toCodePoints(test)), test);
}

function passThrough(codePoint) {
    var string = String.fromCodePoint(codePoint);
    shouldBe(string.codePointAt(0), codePoint);
}

var numberTests = [
    [ 0x10FFFF,  "\uDBFF\uDFFF" ],
    [ 0x10FFFE,  "\uDBFF\uDFFE" ],
    [ 0xFFFF,  "\uFFFF" ],
    [ 0x10000,  "\uD800\uDC00" ],
    [ 0x10001,  "\uD800\uDC01" ],
    [ -0.0,  "\u0000" ],
    [ 0xD800,  "\uD800" ],
    [ 0xDC00,  "\uDC00" ],
];

for (var test of numberTests) {
    shouldBe(String.fromCodePoint(test[0]), test[1]);
}

shouldBe(String.fromCodePoint(0xD800, 0xDC00).codePointAt(0), 0x10000);

// Non-character code points.
for (var i = 0; i < 17; ++i) {
    var plane = 0x10000 * i;
    passThrough(plane + 0xFFFE);
    passThrough(plane + 0xFFFF);
}

for (var start = 0xFDD0; start <= 0xFDEF; ++start) {
    passThrough(start);
}

var invalidTests = [
    -1,
    1.2,
    1.5,
    30.01,
    -11.0,
    NaN,
    Number.Infinity,
    -Number.Infinity,
    0x10FFFF + 1,
    0x7FFFFFFF,
    0x7FFFFFFF + 1,
    0xFFFFFFFF,
    0xFFFFFFFF + 1,
    0x100000000 + 32,  // String.fromCharCode(0x100000000 + 32) produces a space, but String.fromCodePoint should throw an error.
    "Hello",
    undefined,
    {},
];

for (var test of invalidTests) {
    shouldThrow(function () {
        String.fromCodePoint(test);
    }, "RangeError: Arguments contain a value that is out of range of code points");
}

// toNumber causes errors.
shouldThrow(function () {
    String.fromCodePoint(Symbol.iterator);
}, "TypeError: Cannot convert a symbol to a number")

var toNumberObject = {
    valueOf() {
        throw new Error("valueOf is called");
    }
};

shouldThrow(function () {
    String.fromCodePoint(toNumberObject);
}, "Error: valueOf is called")

shouldThrow(function () {
    String.fromCodePoint(Symbol.iterator, toNumberObject);
}, "TypeError: Cannot convert a symbol to a number")

var convertAndPassTests = [
    [ null, "\0" ],
    [ [], "\0" ],
    [ "0x41", "A" ],
    [ "", "\0" ],
    [ true, "\u0001" ],
    [ false, "\u0000" ],
];

for (var test of convertAndPassTests) {
    shouldBe(String.fromCodePoint(test[0]), test[1]);
}
