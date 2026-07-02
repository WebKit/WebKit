function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual} (expected ${expected})`);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("Expected an error to be thrown");
    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name} but got ${error.name}: ${error.message}`);
}

// Latin1 small string range.
function fromCodePointLatin1(cp) {
    return String.fromCodePoint(cp);
}
noInline(fromCodePointLatin1);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(fromCodePointLatin1(0), "\0");
    shouldBe(fromCodePointLatin1(0x41), "A");
    shouldBe(fromCodePointLatin1(0xFF), "ÿ");
}

// BMP non-Latin1 range: still single UTF-16 unit, but not a small string.
function fromCodePointBMP(cp) {
    return String.fromCodePoint(cp);
}
noInline(fromCodePointBMP);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(fromCodePointBMP(0x100), "Ā");
    shouldBe(fromCodePointBMP(0x3042), "あ");
    shouldBe(fromCodePointBMP(0xFFFF), "￿");
}

// Supplementary plane: surrogate pair.
function fromCodePointSupplementary(cp) {
    return String.fromCodePoint(cp);
}
noInline(fromCodePointSupplementary);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(fromCodePointSupplementary(0x10000), "𐀀");
    shouldBe(fromCodePointSupplementary(0x1F600), "😀");
    shouldBe(fromCodePointSupplementary(0x10FFFF), "􏿿");
}

// Out-of-range RangeError after warm-up with valid Int32 inputs.
function fromCodePointMaybeThrows(cp) {
    return String.fromCodePoint(cp);
}
noInline(fromCodePointMaybeThrows);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(fromCodePointMaybeThrows(0x42), "B");
shouldThrow(() => fromCodePointMaybeThrows(-1), RangeError);
shouldThrow(() => fromCodePointMaybeThrows(0x110000), RangeError);
shouldThrow(() => fromCodePointMaybeThrows(0x7FFFFFFF), RangeError);

// Untyped (double) inputs after warm-up.
function fromCodePointDouble(cp) {
    return String.fromCodePoint(cp);
}
noInline(fromCodePointDouble);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(fromCodePointDouble(0x41 + 0.0), "A");
shouldBe(fromCodePointDouble(0x10000 + 0.0), "𐀀");
shouldThrow(() => fromCodePointDouble(0.5), RangeError);
shouldThrow(() => fromCodePointDouble(NaN), RangeError);
shouldThrow(() => fromCodePointDouble(Infinity), RangeError);
shouldThrow(() => fromCodePointDouble(-1.0), RangeError);

// Untyped object input that triggers valueOf side effects.
function fromCodePointObject(cp) {
    return String.fromCodePoint(cp);
}
noInline(fromCodePointObject);

let valueOfCalls = 0;
const obj = { valueOf() { ++valueOfCalls; return 0x43; } };
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(fromCodePointObject(obj), "C");
shouldBe(valueOfCalls, testLoopCount);

// Multi-argument form must still go through the host function.
function fromCodePointMulti(a, b, c) {
    return String.fromCodePoint(a, b, c);
}
noInline(fromCodePointMulti);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(fromCodePointMulti(0x41, 0x42, 0x43), "ABC");

// Result-unused call must still throw RangeError (cannot be DCE'd) even with Int32 input.
function fromCodePointDead(cp) {
    String.fromCodePoint(cp);
    return cp;
}
noInline(fromCodePointDead);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(fromCodePointDead(0x41), 0x41);
shouldThrow(() => fromCodePointDead(0x110000), RangeError);
