function shouldBe(actual, expected, input) {
    if (actual !== expected)
        throw new Error(`bad value for ${escape(input)}: expected ${escape(expected)} got ${escape(actual)}`);
}

function toLowerCase(string) { return string.toLowerCase(); }
noInline(toLowerCase);

// U+0101 is outside of the ranges that are handled without ICU.
const forceICU = "\u0101";
function toLowerCaseWithICU(string) { return toLowerCase(string + forceICU).slice(0, -1); }

for (let start = 0; start < 0x10000; start += 16) {
    let all = "";
    for (let code = start; code < start + 16; ++code) {
        const character = String.fromCharCode(code);
        all += character;
        shouldBe(toLowerCase(character), toLowerCaseWithICU(character), character);
    }
    shouldBe(toLowerCase(all + "A"), toLowerCaseWithICU(all + "A"), all);
}

const boundaries = [0x7f, 0x80, 0x1fff, 0x2000, 0x206f, 0x2070, 0x2fff, 0x3000, 0xa4cf, 0xa4d0, 0xabff, 0xac00, 0xd7a3, 0xd7a4, 0xdfff, 0xe000, 0xfaff, 0xfb00, 0xff00, 0xff01, 0xff20, 0xff21];
for (const code of boundaries) {
    const character = String.fromCharCode(code);
    for (const length of [7, 8, 9, 15, 16, 17]) {
        for (let index = 0; index < length; ++index) {
            const string = "\u65e5".repeat(index) + character + "\u65e5".repeat(length - index - 1);
            shouldBe(toLowerCase(string), toLowerCaseWithICU(string), string);
        }
    }
}

const cased = [
    ["A", "a"],
    ["\u00c9", "\u00e9"],
    ["\u0416", "\u0436"],
    ["\u1e00", "\u1e01"],
    ["\u2160", "\u2170"],
    ["\u2c80", "\u2c81"],
    ["\ua640", "\ua641"],
    ["\uff21", "\uff41"],
];
for (const [upper, lower] of cased) {
    for (const length of [7, 8, 9, 15, 16, 17]) {
        for (let index = 0; index < length; ++index) {
            for (const ascii of ["", "already lower "]) {
                const prefix = ascii + "\u65e5".repeat(index);
                const suffix = "\u65e5".repeat(length - index - 1);
                shouldBe(toLowerCase(prefix + upper + suffix), prefix + lower + suffix, prefix + upper + suffix);
            }
        }
    }
}

const cases = [
    ["\u65e5\u672c\u8a9eABC", "\u65e5\u672c\u8a9eabc"],
    ["Z\u65e5\u672c\u8a9e@[`{", "z\u65e5\u672c\u8a9e@[`{"],
    ["\ud55c\uae00 ABC \u65e5\u672c\u8a9e", "\ud55c\uae00 abc \u65e5\u672c\u8a9e"],
    ["IT\u2019S \u201cOK\u201d\u2026", "it\u2019s \u201cok\u201d\u2026"],
    ["\u65e5\u672c\uff01ABC\uff10", "\u65e5\u672c\uff01abc\uff10"],
    ["it\u2019s Not Lowercase", "it\u2019s not lowercase"],
    ["it\u2019s already lowercase", "it\u2019s already lowercase"],
    ["\u65e5\u672c\u8a9e\uff21", "\u65e5\u672c\u8a9e\uff41"],
    ["\u65e5\u672c\u8a9eABC\u0130", "\u65e5\u672c\u8a9eabci\u0307"],
    ["\u65e5\u672c\u8a9eABC\u{10400}", "\u65e5\u672c\u8a9eabc\u{10428}"],
    ["\u65e5\u672c\u8a9eABC\ud800", "\u65e5\u672c\u8a9eabc\ud800"],
    ["\u65e5\u03a3", "\u65e5\u03c3"],
    ["A\u03a3\u65e5", "a\u03c2\u65e5"],
    ["A\u3005\u03a3", "a\u3005\u03c2"],
    ["A\u03a3\u3005B", "a\u03c3\u3005b"],
    ["A\u2019\u03a3", "a\u2019\u03c2"],
];
for (let i = 0; i < testLoopCount; ++i) {
    for (const [input, expected] of cases)
        shouldBe(toLowerCase(input), expected, input);
}
