// Test that all Unicode Zs (Space_Separator) category characters are correctly
// recognized as whitespace by String.prototype.trim/trimStart/trimEnd.
// Reference: https://util.unicode.org/UnicodeJsps/list-unicodeset.jsp?a=%5B%3AGeneral_Category%3DSpace_Separator%3A%5D

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ': expected ' + JSON.stringify(expected) + ' but got ' + JSON.stringify(actual));
}

// All 17 Unicode Zs (Space_Separator) category characters
const zsCharacters = [
    { code: 0x0020, name: "SPACE" },
    { code: 0x00A0, name: "NO-BREAK SPACE" },
    { code: 0x1680, name: "OGHAM SPACE MARK" },
    { code: 0x2000, name: "EN QUAD" },
    { code: 0x2001, name: "EM QUAD" },
    { code: 0x2002, name: "EN SPACE" },
    { code: 0x2003, name: "EM SPACE" },
    { code: 0x2004, name: "THREE-PER-EM SPACE" },
    { code: 0x2005, name: "FOUR-PER-EM SPACE" },
    { code: 0x2006, name: "SIX-PER-EM SPACE" },
    { code: 0x2007, name: "FIGURE SPACE" },
    { code: 0x2008, name: "PUNCTUATION SPACE" },
    { code: 0x2009, name: "THIN SPACE" },
    { code: 0x200A, name: "HAIR SPACE" },
    { code: 0x202F, name: "NARROW NO-BREAK SPACE" },
    { code: 0x205F, name: "MEDIUM MATHEMATICAL SPACE" },
    { code: 0x3000, name: "IDEOGRAPHIC SPACE" },
];

// BOM is also ECMAScript WhiteSpace (not Zs, but included in spec)
const bomCharacter = { code: 0xFEFF, name: "BYTE ORDER MARK (BOM)" };

// Test each Zs character
for (const { code, name } of zsCharacters) {
    const ws = String.fromCodePoint(code);
    const testStr = ws + "hello" + ws;

    shouldBe(testStr.trim(), "hello", `trim() with U+${code.toString(16).toUpperCase().padStart(4, '0')} ${name}`);
    shouldBe(testStr.trimStart(), "hello" + ws, `trimStart() with U+${code.toString(16).toUpperCase().padStart(4, '0')} ${name}`);
    shouldBe(testStr.trimEnd(), ws + "hello", `trimEnd() with U+${code.toString(16).toUpperCase().padStart(4, '0')} ${name}`);
}

// Test BOM
{
    const ws = String.fromCodePoint(bomCharacter.code);
    const testStr = ws + "hello" + ws;

    shouldBe(testStr.trim(), "hello", `trim() with U+${bomCharacter.code.toString(16).toUpperCase().padStart(4, '0')} ${bomCharacter.name}`);
    shouldBe(testStr.trimStart(), "hello" + ws, `trimStart() with U+${bomCharacter.code.toString(16).toUpperCase().padStart(4, '0')} ${bomCharacter.name}`);
    shouldBe(testStr.trimEnd(), ws + "hello", `trimEnd() with U+${bomCharacter.code.toString(16).toUpperCase().padStart(4, '0')} ${bomCharacter.name}`);
}

// Test other ECMAScript WhiteSpace characters (not Zs but in spec)
const otherWhitespace = [
    { code: 0x0009, name: "CHARACTER TABULATION (TAB)" },
    { code: 0x000B, name: "LINE TABULATION (VT)" },
    { code: 0x000C, name: "FORM FEED (FF)" },
];

for (const { code, name } of otherWhitespace) {
    const ws = String.fromCodePoint(code);
    const testStr = ws + "hello" + ws;

    shouldBe(testStr.trim(), "hello", `trim() with U+${code.toString(16).toUpperCase().padStart(4, '0')} ${name}`);
    shouldBe(testStr.trimStart(), "hello" + ws, `trimStart() with U+${code.toString(16).toUpperCase().padStart(4, '0')} ${name}`);
    shouldBe(testStr.trimEnd(), ws + "hello", `trimEnd() with U+${code.toString(16).toUpperCase().padStart(4, '0')} ${name}`);
}

// Test that non-whitespace characters are NOT trimmed
const nonWhitespace = [
    { code: 0x200B, name: "ZERO WIDTH SPACE" },           // Cf, not Zs
    { code: 0x200C, name: "ZERO WIDTH NON-JOINER" },      // Cf
    { code: 0x200D, name: "ZERO WIDTH JOINER" },          // Cf
    { code: 0x2060, name: "WORD JOINER" },                // Cf
    { code: 0x180E, name: "MONGOLIAN VOWEL SEPARATOR" },  // Cf (was Zs before Unicode 6.3)
];

for (const { code, name } of nonWhitespace) {
    const ch = String.fromCodePoint(code);
    const testStr = ch + "hello" + ch;

    shouldBe(testStr.trim(), testStr, `trim() should NOT trim U+${code.toString(16).toUpperCase().padStart(4, '0')} ${name}`);
    shouldBe(testStr.trimStart(), testStr, `trimStart() should NOT trim U+${code.toString(16).toUpperCase().padStart(4, '0')} ${name}`);
    shouldBe(testStr.trimEnd(), testStr, `trimEnd() should NOT trim U+${code.toString(16).toUpperCase().padStart(4, '0')} ${name}`);
}

// Test mixed Zs characters
{
    const mixed = "\u3000\u2000\u202F\u1680hello\u1680\u202F\u2000\u3000";
    shouldBe(mixed.trim(), "hello", "trim() with mixed Zs characters");
    shouldBe(mixed.trimStart(), "hello\u1680\u202F\u2000\u3000", "trimStart() with mixed Zs characters");
    shouldBe(mixed.trimEnd(), "\u3000\u2000\u202F\u1680hello", "trimEnd() with mixed Zs characters");
}

// Test all Zs characters combined
{
    let allZs = "";
    for (const { code } of zsCharacters) {
        allZs += String.fromCodePoint(code);
    }
    const testStr = allZs + "hello" + allZs;

    shouldBe(testStr.trim(), "hello", "trim() with all Zs characters combined");
    shouldBe(testStr.trimStart(), "hello" + allZs, "trimStart() with all Zs characters combined");
    shouldBe(testStr.trimEnd(), allZs + "hello", "trimEnd() with all Zs characters combined");
}
