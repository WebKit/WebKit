// Test that the Yarr string list fast path correctly matches an empty
// alternative that is not the last alternative, e.g. /^(?:c||b)/.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

function shouldBeMatch(result, expectedStrings, expectedIndex, message) {
    if (expectedStrings === null) {
        shouldBe(result, null, message);
        return;
    }
    if (result === null)
        throw new Error((message ? message + ": " : "") + "expected a match but got null");
    shouldBe(result.length, expectedStrings.length, message + " (length)");
    for (var i = 0; i < expectedStrings.length; ++i)
        shouldBe(result[i], expectedStrings[i], message + " [" + i + "]");
    shouldBe(result.index, expectedIndex, message + " (index)");
}

// Empty alternative in the middle.
shouldBeMatch(/^(?:c||b)/.exec("bccc"), [""], 0, "/^(?:c||b)/ vs \"bccc\"");
shouldBeMatch(/^(?:c||b)/.exec("cccc"), ["c"], 0, "/^(?:c||b)/ vs \"cccc\"");
shouldBeMatch(/^(?:c||b)/.exec("zzzz"), [""], 0, "/^(?:c||b)/ vs \"zzzz\"");
shouldBeMatch(/^(?:c||b)/.exec(""), [""], 0, "/^(?:c||b)/ vs \"\"");

// Empty alternative first.
shouldBeMatch(/^(?:|c)/.exec("c"), [""], 0, "/^(?:|c)/ vs \"c\"");
shouldBeMatch(/^(?:|foo|bar)/.exec("bar"), [""], 0, "/^(?:|foo|bar)/ vs \"bar\"");

// Multiple empty alternatives.
shouldBeMatch(/^(?:||b)/.exec("b"), [""], 0, "/^(?:||b)/ vs \"b\"");

// Empty alternative last already worked; check for regressions.
shouldBeMatch(/^(?:c|b|)/.exec("bccc"), ["b"], 0, "/^(?:c|b|)/ vs \"bccc\"");
shouldBeMatch(/^(?:c|b|)/.exec("zzzz"), [""], 0, "/^(?:c|b|)/ vs \"zzzz\"");

// Multi-character alternatives.
shouldBeMatch(/^(?:foo||bar)/.exec("bar"), [""], 0, "/^(?:foo||bar)/ vs \"bar\"");
shouldBeMatch(/^(?:foo||bar)/.exec("foofoo"), ["foo"], 0, "/^(?:foo||bar)/ vs \"foofoo\"");

// EOL anchored string lists: alternatives after an empty one stay reachable.
shouldBeMatch(/^(?:c||b)$/.exec("c"), ["c"], 0, "/^(?:c||b)$/ vs \"c\"");
shouldBeMatch(/^(?:c||b)$/.exec("b"), ["b"], 0, "/^(?:c||b)$/ vs \"b\"");
shouldBeMatch(/^(?:c||b)$/.exec(""), [""], 0, "/^(?:c||b)$/ vs \"\"");
shouldBeMatch(/^(?:c||b)$/.exec("bc"), null, 0, "/^(?:c||b)$/ vs \"bc\"");

// Not a string list: no BOL anchor.
shouldBeMatch(/(?:c||b)/.exec("bccc"), [""], 0, "/(?:c||b)/ vs \"bccc\"");

// Not a string list: capturing group.
shouldBeMatch(/^(c||b)/.exec("bccc"), ["", ""], 0, "/^(c||b)/ vs \"bccc\"");

// Not a string list: quantified group. A zero-length iteration is rejected
// by RepeatMatcher, so the "b" alternative matches.
shouldBeMatch(/^(?:c||b)?/.exec("bccc"), ["b"], 0, "/^(?:c||b)?/ vs \"bccc\"");

// String list without empty alternatives: fast path regression check.
shouldBeMatch(/^(?:foo|bar|baz)/.exec("barxx"), ["bar"], 0, "/^(?:foo|bar|baz)/ vs \"barxx\"");
shouldBeMatch(/^(?:foo|bar|baz)/.exec("qux"), null, 0, "/^(?:foo|bar|baz)/ vs \"qux\"");

// Flags.
shouldBeMatch(/^(?:c||b)/i.exec("Bccc"), [""], 0, "/^(?:c||b)/i vs \"Bccc\"");
shouldBeMatch(/^(?:c||b)/m.exec("x\nbc"), [""], 0, "/^(?:c||b)/m vs \"x\\nbc\"");

var stickyRe = /^(?:c||b)/y;
stickyRe.lastIndex = 0;
shouldBeMatch(stickyRe.exec("bccc"), [""], 0, "/^(?:c||b)/y vs \"bccc\"");
shouldBe(stickyRe.lastIndex, 0, "sticky lastIndex after zero-length match");

var globalRe = /^(?:c||b)/g;
globalRe.lastIndex = 0;
shouldBeMatch(globalRe.exec("bccc"), [""], 0, "/^(?:c||b)/g vs \"bccc\"");
shouldBe(globalRe.lastIndex, 0, "global lastIndex after zero-length match");

// 16-bit strings.
shouldBeMatch(/^(?:あ||い)/.exec("いい"), [""], 0, "/^(?:\\u3042||\\u3044)/ vs \"\\u3044\\u3044\"");
shouldBeMatch(/^(?:あ||い)/.exec("ああ"), ["あ"], 0, "/^(?:\\u3042||\\u3044)/ vs \"\\u3042\\u3042\"");

// test() and replace() use the same match.
shouldBe(/^(?:c||b)/.test("zzz"), true, "/^(?:c||b)/.test(\"zzz\")");
shouldBe("bccc".replace(/^(?:c||b)/, "X"), "Xbccc", "replace with /^(?:c||b)/");
