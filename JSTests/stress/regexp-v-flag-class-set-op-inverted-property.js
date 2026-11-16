function shouldBe(actual, expected, description) {
    if (actual !== expected)
        throw new Error(`${description}: expected ${expected} but got ${actual}`);
}

function shouldThrowSyntaxError(pattern) {
    let error = null;
    try {
        new RegExp(pattern, "v");
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`/${pattern}/v: expected SyntaxError but got ${error}`);
}

function test(pattern, input, expected, flags = "v") {
    const regexp = new RegExp(pattern, flags);
    shouldBe(regexp.test(input), expected, `/${pattern}/${flags}.test(${JSON.stringify(input)})`);
}

// \P{...} as the right-hand operand of an intersection.
test("^[\\P{Number}&&\\P{Alphabetic}]$", "A", false);
test("^[\\P{Number}&&\\P{Alphabetic}]$", "1", false);
test("^[\\P{Number}&&\\P{Alphabetic}]$", "!", true);
test("^[\\p{L}&&\\P{Lu}]$", "A", false);
test("^[\\p{L}&&\\P{Lu}]$", "a", true);
test("^[\\p{L}&&\\P{Lu}]$", "1", false);
test("^[[a-z]&&\\P{Lu}]$", "a", true);
test("^[[a-z]&&\\P{Lu}]$", "1", false);
test("^[\\P{Lu}&&\\P{Ll}&&\\P{N}]$", "!", true);
test("^[\\P{Lu}&&\\P{Ll}&&\\P{N}]$", "A", false);
test("^[\\P{Lu}&&\\P{Ll}&&\\P{N}]$", "a", false);
test("^[\\P{Lu}&&\\P{Ll}&&\\P{N}]$", "1", false);
test("^[\\p{Any}&&\\P{ASCII}]$", "\u{10000}", true);
test("^[\\p{Any}&&\\P{ASCII}]$", "a", false);

// \P{...} as the right-hand operand of a subtraction.
test("^[\\P{Lu}--\\P{L}]$", "a", true);
test("^[\\P{Lu}--\\P{L}]$", "A", false);
test("^[\\P{Lu}--\\P{L}]$", "1", false);
test("^[\\p{ASCII}--\\P{L}]$", "a", true);
test("^[\\p{ASCII}--\\P{L}]$", "1", false);
test("^[\\p{Any}--\\P{ASCII}]$", "a", true);
test("^[\\p{Any}--\\P{ASCII}]$", "\u{10000}", false);
test("^[\\p{Any}--\\P{L}--\\P{Lu}]$", "A", true);
test("^[\\p{Any}--\\P{L}--\\P{Lu}]$", "a", false);
test("^[\\p{Any}--\\P{L}--\\P{Lu}]$", "1", false);

// Complement code points around the Latin-1 / non-Latin-1 split and isolated complement code points inside Latin-1.
// U+00FF and U+0100 are both letters; U+00D7 and U+00F7 are non-letters surrounded by letters; U+00B5 is a letter surrounded by non-letters.
test("^[\\p{Any}&&\\P{L}]$", "\xff", false);
test("^[\\p{Any}&&\\P{L}]$", "Ā", false);
test("^[\\p{Any}&&\\P{L}]$", "\xd7", true);
test("^[\\p{Any}&&\\P{L}]$", "\xf7", true);
test("^[\\p{Any}&&\\P{L}]$", "\xb5", false);
test("^[\\p{Any}&&\\P{L}]$", "\xb4", true);
test("^[\\p{Any}&&\\P{L}]$", "\xb6", true);
test("^[\\p{Any}--\\P{L}]$", "\xff", true);
test("^[\\p{Any}--\\P{L}]$", "Ā", true);
test("^[\\p{Any}--\\P{L}]$", "\xf7", false);
test("^[\\p{Any}--\\P{L}]$", "\xb5", true);
test("^[\\p{Any}--\\P{L}]$", "\xb6", false);

// The inverted property has no Latin-1 code points at all, so its complement covers all of Latin-1.
test("^[\\p{Any}&&\\P{Script=Greek}]$", "a", true);
test("^[\\p{Any}&&\\P{Script=Greek}]$", "\xff", true);
test("^[\\p{Any}&&\\P{Script=Greek}]$", "α", false);
test("^[\\p{Any}&&\\P{Script=Greek}]$", "\u{10000}", true);
test("^[[a-zα-ω]--\\P{Script=Greek}]$", "α", true);
test("^[[a-zα-ω]--\\P{Script=Greek}]$", "a", false);

// The inverted property covers everything, so its complement is empty.
test("^[\\p{L}&&\\P{Any}]$", "a", false);
test("^[\\p{L}&&\\P{Any}]$", "\u{10000}", false);
test("^[\\p{L}--\\P{Any}]$", "a", true);
test("^[\\p{L}--\\P{Any}]$", "\u{10400}", true);
test("^[\\p{L}--\\P{Any}]$", "1", false);

// \P{...} as the second and later operand of an explicit union (juxtaposition).
test("^[0\\P{L}]$", "0", true);
test("^[0\\P{L}]$", "1", true);
test("^[0\\P{L}]$", "a", false);
test("^[0\\P{L}\\P{N}]$", "a", true);
test("^[0\\P{L}\\P{N}]$", "0", true);
test("^[\\q{ab}\\P{L}]$", "ab", true);
test("^[\\q{ab}\\P{L}]$", "1", true);
test("^[\\q{ab}\\P{L}]$", "a", false);

// The left-hand operand contains strings.
test("^[\\q{ab|c|1}&&\\P{L}]$", "1", true);
test("^[\\q{ab|c|1}&&\\P{L}]$", "ab", false);
test("^[\\q{ab|c|1}&&\\P{L}]$", "c", false);
test("^[\\q{ab|c|1}--\\P{L}]$", "ab", true);
test("^[\\q{ab|c|1}--\\P{L}]$", "c", true);
test("^[\\q{ab|c|1}--\\P{L}]$", "1", false);

// Inside a nested class and under an outer negation.
test("^[[\\p{L}&&\\P{Lu}]1]$", "a", true);
test("^[[\\p{L}&&\\P{Lu}]1]$", "1", true);
test("^[[\\p{L}&&\\P{Lu}]1]$", "A", false);
test("^[\\p{N}&&[\\p{Any}--\\P{Nd}]]$", "1", true);
test("^[\\p{N}&&[\\p{Any}--\\P{Nd}]]$", "\xb2", false);
test("^[^\\p{L}&&\\P{Lu}]$", "A", true);
test("^[^\\p{L}&&\\P{Lu}]$", "1", true);
test("^[^\\p{L}&&\\P{Lu}]$", "a", false);

// With the ignoreCase flag, using properties that have no case mappings.
test("^[\\p{N}&&\\P{Nd}]$", "\xb2", true, "vi");
test("^[\\p{N}&&\\P{Nd}]$", "1", false, "vi");
test("^[\\p{N}&&\\P{Nd}]$", "a", false, "vi");
test("^[\\p{N}--\\P{Nd}]$", "1", true, "vi");
test("^[\\p{N}--\\P{Nd}]$", "\xb2", false, "vi");

// A negated property of strings is still a SyntaxError when it is the right-hand operand of a set operation.
shouldThrowSyntaxError("[\\p{L}&&\\P{RGI_Emoji}]");
shouldThrowSyntaxError("[\\p{L}--\\P{RGI_Emoji}]");

// \P{...} as the left-hand operand or wrapped in a nested class was already correct.
test("^[\\P{L}&&\\p{ASCII}]$", "1", true);
test("^[\\P{L}&&\\p{ASCII}]$", "a", false);
test("^[\\P{L}&&[0-9]]$", "1", true);
test("^[\\P{L}&&[0-9]]$", "!", false);
test("^[[\\P{L}]&&[\\P{N}]]$", "!", true);
test("^[[\\P{L}]&&[\\P{N}]]$", "1", false);
test("^[\\p{L}&&[\\P{Lu}]]$", "a", true);
test("^[\\p{L}&&[\\P{Lu}]]$", "A", false);
