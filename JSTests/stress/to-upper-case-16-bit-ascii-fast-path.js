function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${JSON.stringify(expected)} got ${JSON.stringify(actual)}`);
}

// The JIT scans a string for characters that need converting and returns the string itself when
// there are none. Cover that scan for 16-bit strings, whose content may still be pure ASCII.
const nonLatin1 = "\u2014"; // EM DASH

function toUpperCase(string) { return string.toUpperCase(); }
noInline(toUpperCase);
function uppercasesToItself(string) { return string.toUpperCase() === string; }
noInline(uppercasesToItself);

// A 16-bit string with the given content. This throws rather than quietly hand back an 8-bit
// string: a Latin-1 string of one character or none can only be stored as 8 bit.
function wide(string) {
    const result = $vm.make16BitStringIfPossible(string);
    if (is8BitString(result))
        throw new Error(`not a 16-bit string: ${JSON.stringify(string)}`);
    return result;
}

// Tested both as an 8-bit and as a 16-bit string, so none of these is shorter than two
// characters. The leading '0' is that padding, and both of the JIT's range checks ('a' to 'z',
// and 0x00 to 0x7F) leave it alone, so the character after it is what the entry really covers.
const cases = [
    ["0`", "0`"],
    ["0a", "0A"],
    ["0z", "0Z"],
    ["0{", "0{"],
    ["0\x7F", "0\x7F"],
    ["0\x80", "0\x80"],
    ["0\u00C9", "0\u00C9"],
    ["0\u00E9", "0\u00C9"],
    ["0\u00DF", "0SS"],
    ["0\uFB01", "0FI"],
    ["AB", "AB"],
    ["ab", "AB"],
    ["ALREADY UPPER 123", "ALREADY UPPER 123"],
    ["Mixed Case Here", "MIXED CASE HERE"],
    ["TRAILINGa", "TRAILINGA"],
    ["aTRAILING", "ATRAILING"],
];

// Too short to be stored as 16 bit, so these only reach the JIT's 8-bit scan.
const narrowOnlyCases = [
    ["", ""],
    ["A", "A"],
    ["a", "A"],
    ["Z", "Z"],
    ["z", "Z"],
    ["\u00DF", "SS"],
];

const all = [];
for (const [input, expected] of cases) {
    all.push([input, expected]);
    all.push([wide(input), expected]);
}
for (const [input, expected] of narrowOnlyCases)
    all.push([input, expected]);
// A single character outside Latin-1 is stored as 16 bit without any help.
all.push([nonLatin1, nonLatin1]);
all.push(["\uFB01", "FI"]);
all.push(["a" + nonLatin1, "A" + nonLatin1]);
all.push([nonLatin1 + "abc", nonLatin1 + "ABC"]);

for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    for (const [input, expected] of all)
        shouldBe(toUpperCase(input), expected);
}

const identity = ["ALREADY UPPER", wide("ALREADY UPPER"), nonLatin1, wide("0\x7F")];
const notIdentity = ["Already Upper", wide("Already Upper"), "\u00E9", wide("0\u00E9")];
for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    for (const string of identity)
        shouldBe(uppercasesToItself(string), true);
    for (const string of notIdentity)
        shouldBe(uppercasesToItself(string), false);
}

function toUpperCaseRope(a, b) { return (a + b).toUpperCase(); }
noInline(toUpperCaseRope);
for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    shouldBe(toUpperCaseRope("abc", "DEF"), "ABCDEF");
    shouldBe(toUpperCaseRope("ABC", nonLatin1), "ABC" + nonLatin1);
    shouldBe(toUpperCaseRope("abc", nonLatin1), "ABC" + nonLatin1);
}
