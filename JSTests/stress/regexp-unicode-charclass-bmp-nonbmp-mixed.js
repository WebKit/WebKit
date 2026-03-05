// This test checks that Unicode regular expressions with character classes containing
// both BMP and non-BMP characters correctly match Char16 strings after previously
// matching Char8 strings. A bug in optimizeAlternative caused the term order in
// pattern.m_body to be swapped during Char8 JIT compilation; if the JIT allocation
// subsequently failed the swapped pattern was passed to byteCodeCompilePattern,
// causing the bytecode interpreter to misread surrogate pairs when executing against
// Char16 strings.

function shouldBe(actual, expected, description)
{
    if (actual !== expected)
        throw new Error(description + ": expected " + expected + " but got " + actual);
}

function shouldMatch(re, str)
{
    let result = re.test(str);
    if (!result)
        throw new Error(re + ".test(" + JSON.stringify(str) + ") should be true but was false");
}

function shouldNotMatch(re, str)
{
    let result = re.test(str);
    if (result)
        throw new Error(re + ".test(" + JSON.stringify(str) + ") should be false but was true");
}

// Test 1: BMP + non-BMP mixed CharacterClass followed by fixed characters.
// Char8 match first, then Char16 match (the original bug scenario).
{
    let re = /[a\u{10000}]bcd/u;
    for (let i = 0; i < 100; i++) {
        shouldMatch(re, "abcd");             // Char8: BMP char matches
        shouldMatch(re, "\u{10000}bcd");     // Char16: non-BMP char matches
        shouldNotMatch(re, "Xbcd");          // no match
        shouldNotMatch(re, "\u{10001}bcd");  // different non-BMP char, no match
    }
}

// Test 2: Char16 match first, then Char8 match (reverse order).
{
    let re = /[a\u{10000}]bcd/u;
    for (let i = 0; i < 100; i++) {
        shouldMatch(re, "\u{10000}bcd");     // Char16 first
        shouldMatch(re, "abcd");             // Char8 second
    }
}

// Test 3: Non-BMP character in CharacterClass with multiple BMP alternatives.
{
    let re = /[abc\u{10001}]xyz/u;
    for (let i = 0; i < 100; i++) {
        shouldMatch(re, "axyz");
        shouldMatch(re, "bxyz");
        shouldMatch(re, "cxyz");
        shouldMatch(re, "\u{10001}xyz");
        shouldNotMatch(re, "dxyz");
        shouldNotMatch(re, "\u{10000}xyz");
    }
}

// Test 4: Multiple non-BMP characters in CharacterClass.
{
    let re = /[\u{10000}\u{10001}]bcd/u;
    for (let i = 0; i < 100; i++) {
        shouldMatch(re, "\u{10000}bcd");
        shouldMatch(re, "\u{10001}bcd");
        shouldNotMatch(re, "abcd");          // BMP char not in class
    }
}

// Test 5: BMP + non-BMP CharacterClass at end of pattern.
{
    let re = /abc[d\u{10000}]/u;
    for (let i = 0; i < 100; i++) {
        shouldMatch(re, "abcd");
        shouldMatch(re, "abc\u{10000}");
        shouldNotMatch(re, "abce");
    }
}

// Test 6: Pattern with BMP + non-BMP CharacterClass preceded by fixed characters.
{
    let re = /abc[d\u{10000}]efg/u;
    for (let i = 0; i < 100; i++) {
        shouldMatch(re, "abcdefg");
        shouldMatch(re, "abc\u{10000}efg");
        shouldNotMatch(re, "abcXefg");
    }
}

// Test 7: exec() returning match array with correct index.
{
    let re = /[a\u{10000}]bcd/u;
    for (let i = 0; i < 100; i++) {
        let m8 = re.exec("XabcdY");
        shouldBe(m8[0], "abcd", "exec Char8 match string");
        shouldBe(m8.index, 1, "exec Char8 match index");

        let m16 = re.exec("X\u{10000}bcdY");
        shouldBe(m16[0], "\u{10000}bcd", "exec Char16 match string");
        shouldBe(m16.index, 1, "exec Char16 match index");
    }
}

// Test 8: Inverted CharacterClass with non-BMP character.
{
    let re = /[^a\u{10000}]bcd/u;
    for (let i = 0; i < 100; i++) {
        shouldMatch(re, "Xbcd");
        shouldNotMatch(re, "abcd");          // 'a' is in class
        shouldNotMatch(re, "\u{10000}bcd");  // U+10000 is in class
    }
}
