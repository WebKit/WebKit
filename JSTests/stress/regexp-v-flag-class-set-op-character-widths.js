function shouldBe(actual, expected, description) {
    if (actual !== expected)
        throw new Error(`${description}: expected ${expected} but got ${actual}`);
}

function test(pattern, input, expected) {
    const regexp = new RegExp(pattern, "v");
    for (let i = 0; i < 1e4; ++i)
        shouldBe(regexp.test(input), expected, `/${pattern}/v.test(${JSON.stringify(input)})`);
}

// A nested class-set operand merged via union.
test("^[\\u{10001}-\\u{10002}\\p{Ll}]$", "a", true);
test("^[\\u{10001}-\\u{10002}\\p{Ll}]$", "\u{10001}", true);
test("^[\\u{10001}-\\u{10002}\\p{Ll}]$", "A", false);
test("^[[\\u{10000}-\\u{10002}][\\u{ff}]\\u{ff}]$", "\u{10000}", true);
test("^[[\\u{10000}-\\u{10002}][\\u{ff}]\\u{ff}]$", "\u{ff}", true);
test("^[[\\u{10000}-\\u{10002}][\\u{ff}]\\u{ff}]$", "a", false);

// Same as above, but with quantifiers so that the fixed-count fast paths are also exercised.
test("^[\\u{10001}-\\u{10002}\\p{Ll}]{3}$", "a\u{10001}b", true);
test("^[\\u{10001}-\\u{10002}\\p{Ll}]{3}$", "abc", true);
test("^[\\u{10001}-\\u{10002}\\p{Ll}]{3}$", "\u{10001}\u{10002}\u{10001}", true);
test("^[\\u{10001}-\\u{10002}\\p{Ll}]{3}$", "ab", false);

// A single non-BMP character coming from a one character string disjunction.
test("^[\\q{\u{10000}}a]$", "\u{10000}", true);
test("^[\\q{\u{10000}}a]$", "a", true);
test("^[\\q{\u{10000}}a]$", "\u{10001}", false);

// Intersection and subtraction can narrow the character widths of the resulting class.
test("^[[a\\u{10000}]&&[a-z]]$", "a", true);
test("^[[a\\u{10000}]&&[a-z]]$", "\u{10000}", false);
test("^[[a\\u{10000}]--[a-z]]$", "\u{10000}", true);
test("^[[a\\u{10000}]--[a-z]]$", "a", false);
test("^[[a\\u{10000}]--[\\u{10000}-\\u{10001}]]$", "a", true);
test("^[[a\\u{10000}]--[\\u{10000}-\\u{10001}]]$", "\u{10000}", false);

// A negated nested class covers both BMP and non-BMP code points.
test("^[[^a]]$", "b", true);
test("^[[^a]]$", "\u{10000}", true);
test("^[[^a]]$", "a", false);
test("^[[^\\u{10000}]]$", "a", true);
test("^[[^\\u{10000}]]$", "\u{10001}", true);
test("^[[^\\u{10000}]]$", "\u{10000}", false);
