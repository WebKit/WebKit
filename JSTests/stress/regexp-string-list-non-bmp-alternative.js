function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

function shouldBeMatch(result, expected, message) {
    if (expected === null) {
        shouldBe(result, null, message);
        return;
    }
    if (result === null)
        throw new Error((message ? message + ": " : "") + "expected a match but got null");
    shouldBe(result.length, 1, message + " (length)");
    shouldBe(result[0], expected, message + " [0]");
    shouldBe(result.index, 0, message + " (index)");
}

function test(re, subject, expected) {
    // Append a 16-bit character so the subject is a 16-bit string.
    var input = subject + "ā";
    shouldBeMatch(re.exec(input), expected, re + ".exec(" + JSON.stringify(input) + ")");
}

// A non-last alternative containing a non-BMP character must not succeed after
// matching only the characters before it.
test(/^(?:a\u{1F600}|qa)/u, "azk", null);
test(/^(?:a\u{1F600}|qa)/u, "z\u{1F600}", null);
test(/^(?:a\u{1F600}|qa)/u, "a\u{1F600}", "a\u{1F600}");
test(/^(?:a\u{1F600}|qa)/u, "qa", "qa");
test(/^(?:a\u{1F600}|qa)/v, "azk", null);

test(/^(?:a\u{1F600}c|q)/u, "azkc", null);
test(/^(?:a\u{1F600}c|q)/u, "a\u{1F600}z", null);
test(/^(?:a\u{1F600}c|q)/u, "a\u{1F600}c", "a\u{1F600}c");
test(/^(?:a\u{1F600}c|q)/u, "q", "q");

test(/^(?:foo|a\u{1F600}b|bar)/u, "axyz", null);
test(/^(?:foo|a\u{1F600}b|bar)/u, "a\u{1F600}b", "a\u{1F600}b");
test(/^(?:foo|a\u{1F600}b|bar)/u, "bar", "bar");

test(/^(?:abcd\u{1F600}|abcdz)$/u, "abcdā", null);
test(/^(?:abcd\u{1F600}|abcdz)$/u, "abcd\u{1F600}", null);
shouldBeMatch(/^(?:abcd\u{1F600}|abcdz)$/u.exec("abcd\u{1F600}"), "abcd\u{1F600}", "EOL string list exact");
shouldBeMatch(/^(?:abcd\u{1F600}|abcdz)$/u.exec("abcdz"), "abcdz", "EOL string list second alternative");

// Lone surrogate literal splits the run the same way.
test(/^(?:a\uD83D|qa)/u, "azk", null);
test(/^(?:a\uD83D|qa)/u, "qa", "qa");

// Non-BMP character first in the alternative.
test(/^(?:\u{1F600}ab|q)/u, "\u{1F600}az", null);
test(/^(?:\u{1F600}ab|q)/u, "\u{1F600}ab", "\u{1F600}ab");

// Non-BMP character in the last alternative already worked.
test(/^(?:qa|a\u{1F600})/u, "azk", null);
test(/^(?:qa|a\u{1F600})/u, "a\u{1F600}", "a\u{1F600}");
