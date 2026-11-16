// JIT case-insensitive backreference must not use latin1CanonicalizationTable
// for non-ASCII characters. When the captured character is non-ASCII (e.g.,
// U+212A KELVIN SIGN) but the input character is ASCII (e.g., 'K'), the JIT
// fast path incorrectly accessed the table out of bounds.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// U+212A KELVIN SIGN folds to 'K' (U+004B) under Unicode case folding.
// The backreference \1 should match 'K' when ignoreCase and unicode flags are set.
// This was returning false in JIT due to out-of-bounds table access.

// Test 1: Kelvin sign captured, backreference matches ASCII 'K'
for (let i = 0; i < 100; i++) {
    let result = /(\u212a)\1/iu.test("\u212aK");
    shouldBe(result, true, "Test 1: /(\\u212a)\\1/iu.test('\\u212aK')");
}

// Test 2: ASCII 'K' captured, backreference matches Kelvin sign
for (let i = 0; i < 100; i++) {
    let result = /(K)\1/iu.test("K\u212a");
    shouldBe(result, true, "Test 2: /(K)\\1/iu.test('K\\u212a')");
}

// Test 3: Kelvin sign captured, backreference matches lowercase 'k'
for (let i = 0; i < 100; i++) {
    let result = /(\u212a)\1/iu.test("\u212ak");
    shouldBe(result, true, "Test 3: /(\\u212a)\\1/iu.test('\\u212ak')");
}

// U+017F LATIN SMALL LETTER LONG S folds to 's' (U+0073) under Unicode case folding.
// Test 4: Long S captured, backreference matches ASCII 's'
for (let i = 0; i < 100; i++) {
    let result = /(\u017f)\1/iu.test("\u017fs");
    shouldBe(result, true, "Test 4: /(\\u017f)\\1/iu.test('\\u017fs')");
}

// Test 5: Long S captured, backreference matches ASCII 'S'
for (let i = 0; i < 100; i++) {
    let result = /(\u017f)\1/iu.test("\u017fS");
    shouldBe(result, true, "Test 5: /(\\u017f)\\1/iu.test('\\u017fS')");
}

// Test 6: ASCII 'S' captured, backreference matches Long S
for (let i = 0; i < 100; i++) {
    let result = /(S)\1/iu.test("S\u017f");
    shouldBe(result, true, "Test 6: /(S)\\1/iu.test('S\\u017f')");
}

// Test 7: Kelvin sign self-match (both captured and backreference are U+212A)
for (let i = 0; i < 100; i++) {
    let result = /(\u212a)\1/iu.test("\u212a\u212a");
    shouldBe(result, true, "Test 7: /(\\u212a)\\1/iu.test('\\u212a\\u212a')");
}

// Test 8: Verify exec() returns correct match
for (let i = 0; i < 100; i++) {
    let match = /(\u212a)\1/iu.exec("\u212aK");
    shouldBe(match !== null, true, "Test 8: match should not be null");
    shouldBe(match[0], "\u212aK", "Test 8: full match");
    shouldBe(match[1], "\u212a", "Test 8: capture group");
}

// Test 9: Non-matching case still correctly fails
for (let i = 0; i < 100; i++) {
    let result = /(\u212a)\1/iu.test("\u212aX");
    shouldBe(result, false, "Test 9: /(\\u212a)\\1/iu.test('\\u212aX') should not match");
}

// Test 10: Without unicode flag - UCS2 mode.
// In non-Unicode mode, case folding uses simple toUpperCase which does NOT
// equate U+212A (Kelvin sign) with 'K'. So this should NOT match.
for (let i = 0; i < 100; i++) {
    let result = /(\u212a)\1/i.test("\u212aK");
    shouldBe(result, false, "Test 10: /(\\u212a)\\1/i.test('\\u212aK') should not match without u flag");
}
