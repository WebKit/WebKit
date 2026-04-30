// Exercises the StringCharCodeAt / StringCodePointAt rules in
// DFGIntegerRangeOptimizationPhase. The results are known to lie in
// [0, 0xFFFF] and [0, 0x10FFFF] respectively, which lets the phase fold
// subsequent CheckInBounds and narrow downstream ranges.

function assert(actual, expected) {
    if (actual !== expected)
        throw new Error("Expected " + expected + " but got " + actual);
}

// charCodeAt result is in [0, 0xFFFF], so indexing an array sized to that
// range is always in bounds.
function charCodeIndex(str, i, table) {
    return table[str.charCodeAt(i)];
}
noInline(charCodeIndex);

// codePointAt result is in [0, 0x10FFFF]. Combining it with a mask keeps the
// result within a 32-bit signed range without overflow.
function codePointIndex(str, i, table) {
    return table[str.codePointAt(i) & 0xFFFF];
}
noInline(codePointIndex);

// charCodeAt result feeding a bitwise AND: the combined range is the
// intersection of [0, 0xFFFF] and [0, 0xFF].
function charCodeLowByte(str, i) {
    return str.charCodeAt(i) & 0xFF;
}
noInline(charCodeLowByte);

// codePointAt followed by a comparison that is always true given the bound.
function codePointBelowMax(str, i) {
    var c = str.codePointAt(i);
    return c <= 0x10FFFF;
}
noInline(codePointBelowMax);

var asciiTable = new Array(0x10000);
for (var i = 0; i < 0x10000; ++i)
    asciiTable[i] = i;

var bmpTable = new Array(0x10000);
for (var i = 0; i < 0x10000; ++i)
    bmpTable[i] = i * 2;

var str = "Hello, World!";
var supplementary = "a😀b"; // contains U+1F600 (GRINNING FACE).

for (var i = 0; i < testLoopCount; ++i) {
    assert(charCodeIndex(str, 0, asciiTable), 72);
    assert(charCodeIndex(str, 7, asciiTable), 87);
    assert(charCodeIndex(supplementary, 1, asciiTable), 0xD83D);

    assert(codePointIndex(supplementary, 1, bmpTable), (0x1F600 & 0xFFFF) * 2);
    assert(codePointIndex(str, 0, bmpTable), 72 * 2);

    assert(charCodeLowByte(str, 0), 72 & 0xFF);
    assert(charCodeLowByte(supplementary, 1), 0xD83D & 0xFF);

    assert(codePointBelowMax(supplementary, 1), true);
    assert(codePointBelowMax(str, 0), true);
}
