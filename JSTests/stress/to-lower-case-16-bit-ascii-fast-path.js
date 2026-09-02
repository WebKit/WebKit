function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${JSON.stringify(expected)} got ${JSON.stringify(actual)}`);
}

// The JIT scans a string for characters that need converting and returns the string itself when
// there are none. Cover that scan for 16-bit strings, whose content may still be pure ASCII.
const nonLatin1 = "\u2014"; // EM DASH

function toLowerCase(string) { return string.toLowerCase(); }
noInline(toLowerCase);
function lowercasesToItself(string) { return string.toLowerCase() === string; }
noInline(lowercasesToItself);

// A 16-bit string with the given content. This throws rather than quietly hand back an 8-bit
// string: a Latin-1 string of one character or none can only be stored as 8 bit.
function wide(string) {
    const result = $vm.make16BitStringIfPossible(string);
    if (is8BitString(result))
        throw new Error(`not a 16-bit string: ${JSON.stringify(string)}`);
    return result;
}

// Tested both as an 8-bit and as a 16-bit string, so none of these is shorter than two
// characters. The leading '0' is that padding, and both of the JIT's range checks ('A' to 'Z',
// and 0x00 to 0x7F) leave it alone, so the character after it is what the entry really covers.
const cases = [
    ["0@", "0@"],
    ["0A", "0a"],
    ["0Z", "0z"],
    ["0[", "0["],
    ["0\x7F", "0\x7F"],
    ["0\x80", "0\x80"],
    ["0\u00E9", "0\u00E9"],
    ["0\u00C9", "0\u00E9"],
    ["0\u0130", "0i\u0307"],
    ["ab", "ab"],
    ["AB", "ab"],
    ["already lower 123", "already lower 123"],
    ["Mixed Case Here", "mixed case here"],
    ["trailingA", "trailinga"],
    ["Atrailing", "atrailing"],
];

// Too short to be stored as 16 bit, so these only reach the JIT's 8-bit scan.
const narrowOnlyCases = [
    ["", ""],
    ["a", "a"],
    ["A", "a"],
    ["z", "z"],
    ["Z", "z"],
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
all.push(["\u0130", "i\u0307"]);
all.push(["A" + nonLatin1, "a" + nonLatin1]);
all.push([nonLatin1 + "ABC", nonLatin1 + "abc"]);

for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    for (const [input, expected] of all)
        shouldBe(toLowerCase(input), expected);
}

const identity = ["already lower", wide("already lower"), nonLatin1, wide("0\x7F")];
const notIdentity = ["Already Lower", wide("Already Lower"), "\u00C9", wide("0\u00C9")];
for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    for (const string of identity)
        shouldBe(lowercasesToItself(string), true);
    for (const string of notIdentity)
        shouldBe(lowercasesToItself(string), false);
}

function toLowerCaseRope(a, b) { return (a + b).toLowerCase(); }
noInline(toLowerCaseRope);
for (let iteration = 0; iteration < testLoopCount; ++iteration) {
    shouldBe(toLowerCaseRope("ABC", "def"), "abcdef");
    shouldBe(toLowerCaseRope("abc", nonLatin1), "abc" + nonLatin1);
    shouldBe(toLowerCaseRope("ABC", nonLatin1), "abc" + nonLatin1);
}
