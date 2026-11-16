function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

// The first-character fast-fail filter must agree with the RegExp engine even when the argument is
// not speculated to be a string, in which case RegExp.prototype.test has to run ToString first.
const anchored = /^[a-c]([a-z0-9_.]*)$/;
const digits = /^[0-9]+$/;
const sticky = /[a-c][0-9]*/y;

let toStringCalls = 0;
const toStringObject = {
    toString() {
        toStringCalls++;
        return "abc";
    }
};

function testAnchored(input) {
    return anchored.test(input);
}
noInline(testAnchored);

function testDigits(input) {
    return digits.test(input);
}
noInline(testDigits);

function testSticky(input) {
    return sticky.test(input);
}
noInline(testSticky);

function makeRope(a, b) {
    return a + b;
}
noInline(makeRope);

const anchoredCases = [
    ["abc", true],
    ["zzz", false],
    ["", false],
    ["a\u3042", false], // 16-bit string: never filtered.
    ["c_9.x", true],
    [undefined, false],
    [null, false],
    [123, false],
    [true, false],
    [{ }, false],
    [toStringObject, true],
    [new String("bcd"), true],
];

const digitsCases = [
    [123, true],
    [-1, false],
    ["42", true],
    ["x1", false],
    [undefined, false],
    [null, false],
    [{ }, false],
];

for (let i = 0; i < 1e5; ++i) {
    for (let j = 0; j < anchoredCases.length; ++j)
        shouldBe(testAnchored(anchoredCases[j][0]), anchoredCases[j][1]);

    for (let j = 0; j < digitsCases.length; ++j)
        shouldBe(testDigits(digitsCases[j][0]), digitsCases[j][1]);

    // Symbols reach ToString and throw rather than being filtered out.
    let threw = false;
    try {
        testAnchored(Symbol.iterator);
    } catch (error) {
        threw = error instanceof TypeError;
    }
    shouldBe(threw, true);

    // A rope and a 16-bit string are delegated to the operation.
    shouldBe(testAnchored(makeRope("a", "bc")), true);
    shouldBe(testAnchored(makeRope("z", "zz")), false);

    // Sticky matching walks lastIndex forward on success and resets it to 0 on failure.
    sticky.lastIndex = 0;
    shouldBe(testSticky("a1z"), true);
    shouldBe(sticky.lastIndex, 2);
    shouldBe(testSticky("a1z"), false);
    shouldBe(sticky.lastIndex, 0);

    shouldBe(testSticky("zzz"), false);
    shouldBe(sticky.lastIndex, 0);

    shouldBe(testSticky(undefined), false);
    shouldBe(sticky.lastIndex, 0);
    shouldBe(testSticky(1), false);
    shouldBe(sticky.lastIndex, 0);
    shouldBe(testSticky({ }), false);
    shouldBe(sticky.lastIndex, 0);

    sticky.lastIndex = 1;
    shouldBe(testSticky("zb2"), true);
    shouldBe(sticky.lastIndex, 3);

    // lastIndex past the end, and a non-int32 lastIndex, both have to reach the operation.
    sticky.lastIndex = 100;
    shouldBe(testSticky("a1"), false);
    shouldBe(sticky.lastIndex, 0);

    sticky.lastIndex = 0.5;
    shouldBe(testSticky("a1"), true);
    shouldBe(sticky.lastIndex, 2);
}

shouldBe(toStringCalls, 1e5);
