
function shouldBe(actual, expected) {
    if (actual !== expected && !(actual !== null && typeof(expected) === "string" && actual.toString() == expected))
        throw new Error('expected: ' + expected + ', bad value: ' + actual);
}

function shouldThrow(func, expected) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error('not thrown');
    shouldBe(String(error), expected);
}

var errorKey = {
    toString() {
        throw new Error('out');
    }
};
var string = 'Cocoa, Cappuccino, Rize, Matcha, Kilimanjaro';

shouldThrow(function () {
    String.prototype.match.call(null, /Cocoa/);
}, "TypeError: String.prototype.match requires that |this| not be null");

shouldThrow(function () {
    String.prototype.match.call(undefined, /Cocoa/);
}, "TypeError: String.prototype.match requires that |this| not be undefined");

shouldThrow(function () {
    string.match(errorKey);
}, "Error: out");

shouldBe('Cocoa'.match(/Cocoa/), "Cocoa");

shouldBe(string.match(/Rize/), "Rize");
shouldBe(string.match('Rize'), "Rize");
shouldBe(string.match(/Matcha/), "Matcha");
shouldBe(string.match('Matcha'), "Matcha");

shouldBe('    undefined'.match(), "");
shouldBe('    undefined'.match('undefined'), "undefined");

shouldBe((/Cocoa/)[Symbol.match]('Cocoa'), "Cocoa");

var primitives = [
    '',
    'string',
    42,
    Symbol('Cocoa'),
];

for (var primitive of primitives) {
    shouldThrow(function () {
        RegExp.prototype[Symbol.match].call(primitive)
    }, 'TypeError: RegExp.prototype.@@match requires that |this| be an Object');
}

shouldThrow(function () {
    /Cocoa/[Symbol.match](errorKey);
}, "Error: out");


function testRegExpMatch(regexp, string, result, lastIndex) {
    shouldBe(regexp[Symbol.match](string), result);
    shouldBe(regexp.lastIndex, lastIndex);
}

function testMatch(regexp, string, result, lastIndex) {
    shouldBe(string.match(regexp), result);
    shouldBe(regexp.lastIndex, lastIndex);
}

function testBoth(regexp, string, result, lastIndex) {
    testMatch(regexp, string, result, lastIndex);
    testRegExpMatch(regexp, string, result, lastIndex);
}

var cocoa = /Cocoa/;
cocoa.lastIndex = 20;
testBoth(cocoa, 'Cocoa', "Cocoa", 20);
testBoth(cocoa, '  Cocoa', "Cocoa", 20);
testBoth(cocoa, '  ', null, 20);

var multibyte = /ココア/;
multibyte.lastIndex = 20;
testBoth(multibyte, 'ココア', 'ココア', 20);
testBoth(multibyte, '  Cocoa', null, 20);
testBoth(multibyte, 'カプチーノ', null, 20);

function alwaysUnmatch(string) {
    return null;
}

var regexp = new RegExp('ココア');
regexp[Symbol.match] = alwaysUnmatch;
shouldBe(regexp[Symbol.match], alwaysUnmatch);
testBoth(regexp, 'ココア', null, 0);
