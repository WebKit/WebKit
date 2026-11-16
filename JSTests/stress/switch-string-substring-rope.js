// Coverage for the DFG and FTL binary string switch reading a substring rope in place, which
// depends on decoding the rope's base pointer and character offset correctly.

function shouldBe(actual, expected, tag, detail) {
    if (actual !== expected) {
        let where = detail === undefined ? tag : `${tag} [${String(detail)}]`;
        throw new Error(`${where}: expected ${String(expected)} but got ${String(actual)}`);
    }
}

function switchOnKeys(scrutinee) {
    switch (scrutinee) {
    case "a": return "a";
    case "no": return "no";
    case "for": return "for";
    case "fort": return "fort";
    case "forte": return "forte";
    case "with": return "with";
    case "abcdefghij": return "abcdefghij";
    default: return "default";
    }
}
noInline(switchOnKeys);

// Keys appear at several offsets, adjacent to and overlapping one another, so a base pointer or
// character offset that is off by any amount decodes into a different case.
const base8Bit = "xanoforteforwithabcdefghijnofortex";
const base16Bit = $vm.make16BitStringIfPossible(base8Bit);

// Oracle: the same dispatch over a string built character by character, which is resolved rather
// than a rope and so never takes the path under test.
const keyToResult = new Map([
    ["a", "a"], ["no", "no"], ["for", "for"], ["fort", "fort"],
    ["forte", "forte"], ["with", "with"], ["abcdefghij", "abcdefghij"],
]);
function expectedFor(offset, length) {
    let characters = [];
    for (let i = offset; i < Math.min(offset + length, base8Bit.length); ++i)
        characters.push(base8Bit.charCodeAt(i));
    const key = String.fromCharCode(...characters);
    return keyToResult.has(key) ? keyToResult.get(key) : "default";
}

const substringCases = [];
for (let offset = 0; offset <= base8Bit.length; ++offset) {
    for (let length = 0; length <= 11; ++length)
        substringCases.push([offset, length, expectedFor(offset, length)]);
}

// A substring of a substring collapses onto the original base, exercising a decode of a rope whose
// offset is the sum of two slices.
function substringOfSubstring(string) {
    return string.substring(2, 12).substring(2, 5);
}

function check([offset, length, expected]) {
    const detail = `${offset},${length}`;
    shouldBe(switchOnKeys(base8Bit.substring(offset, offset + length)), expected, "8-bit base", detail);
    shouldBe(switchOnKeys(base16Bit.substring(offset, offset + length)), expected, "16-bit base", detail);
    // A concat rope of the same characters must reach the same case.
    shouldBe(switchOnKeys(base8Bit.substring(offset, offset + length) + ""), expected, "concat rope", detail);
}

// Sweep every offset and length once, then keep one rotating case hot so the switch tiers up with
// substring ropes flowing through it.
for (const substringCase of substringCases)
    check(substringCase);

for (let i = 0; i < testLoopCount; ++i) {
    check(substringCases[i % substringCases.length]);
    shouldBe(switchOnKeys(substringOfSubstring(base8Bit)), "for", "substring of substring");
    shouldBe(switchOnKeys(substringOfSubstring(base16Bit)), "for", "substring of substring, 16-bit base");
    // An unsliced substring yields the base itself rather than a rope.
    shouldBe(switchOnKeys("for".substring(0, 3)), "for", "whole-string substring");
}
