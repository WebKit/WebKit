function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL: " + msg + " expected " + JSON.stringify(expected) + " got " + JSON.stringify(actual));
}

// When String.prototype[@@split] is called with an empty string input,
// it performs a RegExpExec on the splitter regexp (spec step 17.a).
// This exec must update the legacy RegExp statics (RegExp.lastMatch,
// RegExp.input, etc.) just like any other RegExpExec call in split.

// Case 1: empty-string pattern matches the empty input → result = []
// The match succeeds at position 0 with an empty match.
// The legacy RegExp statics should reflect this match.
/seed/.test("hello seed world");
shouldBe(RegExp.lastMatch, "seed", "sanity: lastMatch before split");
shouldBe(RegExp.input, "hello seed world", "sanity: input before split");

var r1 = "".split(/(?:)/);
shouldBe(r1.length, 0, "case1: result length");
shouldBe(RegExp.lastMatch, "", "case1: lastMatch after empty-input split (match)");
shouldBe(RegExp.input, "", "case1: input after empty-input split (match)");
shouldBe(RegExp.leftContext, "", "case1: leftContext after empty-input split (match)");
shouldBe(RegExp.rightContext, "", "case1: rightContext after empty-input split (match)");

// Case 2: non-matching pattern on empty input → result = [""]
// The match fails, so the legacy RegExp statics should remain unchanged.
/prev/.test("some prev text");
shouldBe(RegExp.lastMatch, "prev", "sanity: lastMatch before case2");

var r2 = "".split(/nonexistent/);
shouldBe(r2.length, 1, "case2: result length");
shouldBe(r2[0], "", "case2: result[0]");
// performMatch records on success only; a failed match leaves statics unchanged.
shouldBe(RegExp.lastMatch, "prev", "case2: lastMatch unchanged after failed match");
shouldBe(RegExp.input, "some prev text", "case2: input unchanged after failed match");

// Case 3: pattern with capture groups that matches empty input.
// The statics should reflect the match including captures.
/./.test("X");
var r3 = "".split(/(x)?/);
shouldBe(r3.length, 0, "case3: result length");
shouldBe(RegExp.lastMatch, "", "case3: lastMatch");
shouldBe(RegExp.input, "", "case3: input");
shouldBe(RegExp.$1, "", "case3: $1 (undefined capture → empty string)");

// Case 4: consistency with non-empty input — the generic path already
// uses performMatch(), so these are reference values for comparison.
/x/.test("xxx");
"a".split(/(?:)/);
shouldBe(RegExp.lastMatch, "", "case4: non-empty input lastMatch (reference)");
shouldBe(RegExp.input, "a", "case4: non-empty input (reference)");
