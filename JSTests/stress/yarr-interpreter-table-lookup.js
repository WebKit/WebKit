//@ runDefault("--useRegExpJIT=0")

const testStrings = [
    "a", "Z", "5", "_",  // word chars
    "!", " ", "@", "#",  // non-word chars
    " ", "\t", "\n",     // spaces
    "x", "Y", "0"        // non-spaces
];

function testWordChar() {
    const reg = /\w/;
    for (let j = 0; j < testStrings.length; j++) {
        const expected = /[a-zA-Z0-9_]/.test(testStrings[j]);
        if (reg.test(testStrings[j]) !== expected)
            throw new Error(`\\w failed for "${testStrings[j]}"`);
    }
}

function testNonWordChar() {
    const reg = /\W/;
    for (let j = 0; j < testStrings.length; j++) {
        const expected = !/[a-zA-Z0-9_]/.test(testStrings[j]);
        if (reg.test(testStrings[j]) !== expected)
            throw new Error(`\\W failed for "${testStrings[j]}"`);
    }
}

function testSpaces() {
    const reg = /\s/;
    for (let j = 0; j < testStrings.length; j++) {
        const expected = /[ \t\n\r\f\v]/.test(testStrings[j]);
        if (reg.test(testStrings[j]) !== expected)
            throw new Error(`\\s failed for "${testStrings[j]}"`);
    }
}

function testNonSpaces() {
    const reg = /\S/;
    for (let j = 0; j < testStrings.length; j++) {
        const expected = !/[ \t\n\r\f\v]/.test(testStrings[j]);
        if (reg.test(testStrings[j]) !== expected)
            throw new Error(`\\S failed for "${testStrings[j]}"`);
    }
}

function testDigit() {
    const reg = /\d/;
    for (let j = 0; j < testStrings.length; j++) {
        const expected = /[0-9]/.test(testStrings[j]);
        if (reg.test(testStrings[j]) !== expected)
            throw new Error(`\\d failed for "${testStrings[j]}"`);
    }
}

function testNonDigit() {
    const reg = /\D/;
    for (let j = 0; j < testStrings.length; j++) {
        const expected = !/[0-9]/.test(testStrings[j]);
        if (reg.test(testStrings[j]) !== expected)
            throw new Error(`\\D failed for "${testStrings[j]}"`);
    }
}

// Test table boundary (tableSize = 65536)
function testTableBoundary() {
    // U+FFFF (65535) - last character in table range
    const charAtBoundary = String.fromCharCode(0xFFFF);
    // U+10000 (65536) - first character outside table range (surrogate pair)
    const charOutsideBoundary = String.fromCodePoint(0x10000);

    // \w should not match these
    if (/\w/.test(charAtBoundary))
        throw new Error("\\w should not match U+FFFF");
    if (/\w/.test(charOutsideBoundary))
        throw new Error("\\w should not match U+10000");

    // \W should match these
    if (!/\W/.test(charAtBoundary))
        throw new Error("\\W should match U+FFFF");
    if (!/\W/.test(charOutsideBoundary))
        throw new Error("\\W should match U+10000");
}

// Test Unicode whitespace characters within table range
function testUnicodeSpaces() {
    const unicodeSpaces = [
        "\u00A0",  // NO-BREAK SPACE
        "\u1680",  // OGHAM SPACE MARK
        "\u2000",  // EN QUAD
        "\u2001",  // EM QUAD
        "\u2002",  // EN SPACE
        "\u2003",  // EM SPACE
        "\u2028",  // LINE SEPARATOR
        "\u2029",  // PARAGRAPH SEPARATOR
        "\u202F",  // NARROW NO-BREAK SPACE
        "\u205F",  // MEDIUM MATHEMATICAL SPACE
        "\u3000",  // IDEOGRAPHIC SPACE
        "\uFEFF",  // ZERO WIDTH NO-BREAK SPACE
    ];

    for (const sp of unicodeSpaces) {
        if (!/\s/.test(sp))
            throw new Error("\\s should match U+" + sp.charCodeAt(0).toString(16).toUpperCase());
        if (/\S/.test(sp))
            throw new Error("\\S should not match U+" + sp.charCodeAt(0).toString(16).toUpperCase());
    }
}

for (let i = 0; i < 1e4; i++) {
    testWordChar();
    testNonWordChar();
    testSpaces();
    testNonSpaces();
    testDigit();
    testNonDigit();
    testTableBoundary();
    testUnicodeSpaces();
}
