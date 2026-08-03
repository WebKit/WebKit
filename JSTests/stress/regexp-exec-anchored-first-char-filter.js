// Verify the anchored (^, non-multiline) non-sticky RegExp.exec first-character fast-fail filter:
// a filtered no-match must equal the operation's result, and NON-anchored patterns must never be
// filtered (a match can appear anywhere).

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function execStr(re, s) {
    var m = re.exec(s);
    return m === null ? "null" : "[" + m.map(function (x) { return x === undefined ? "u" : JSON.stringify(x); }).join(",") + "]@" + m.index;
}

function step() {
    // Anchored ^ non-sticky exec: match must start at 0, so input[0] gates it.
    shouldBe(execStr(/^foo/, "foobar"), '["foo"]@0');
    shouldBe(execStr(/^foo/, "xfoobar"), "null");        // filtered (x != f)
    shouldBe(execStr(/^foo/, ""), "null");               // empty
    shouldBe(execStr(/^(?:await|case|return)$/, "case"), '["case"]@0');
    shouldBe(execStr(/^(?:await|case|return)$/, ";"), "null");     // filtered
    shouldBe(execStr(/^(?:await|case|return)$/, "config"), "null"); // c passes filter, op fails
    shouldBe(execStr(/^(#?)[0-9]+/, "123"), '["123",""]@0');       // capture group preserved
    shouldBe(execStr(/^(#?)[0-9]+/, "#12"), '["#12","#"]@0');
    shouldBe(execStr(/^(#?)[0-9]+/, "abc"), "null");     // filtered

    // NON-anchored non-sticky exec: MUST NOT be filtered (match can be anywhere).
    shouldBe(execStr(/foo/, "xxfoo"), '["foo"]@2');
    shouldBe(execStr(/[0-9]+/, "abc99"), '["99"]@3');

    // Multiline ^ is not anchored-at-0.
    shouldBe(execStr(/^bar/m, "x\nbar"), '["bar"]@2');

    // A character whose Unicode case-fold partner is ASCII (U+212A KELVIN -> k/K) becomes a folded
    // character class under /iu, so its ASCII partner must be in the bitmap and NOT fast-failed.
    shouldBe(execStr(/^K/iu, "k"), '["k"]@0');
    shouldBe(execStr(/^K/iu, "K"), '["K"]@0');
    shouldBe(execStr(/^K/iu, "x"), "null");
    shouldBe(execStr(/^K/i, "k"), "null");    // non-unicode: KELVIN does not fold to ASCII

    // Global RegExp state after a filtered no-match reflects the last successful match, not the
    // failed one (a no-match exec never records).
    shouldBe(execStr(/^z/, "zzz"), '["z"]@0');
    var savedLastMatch = RegExp.lastMatch;
    shouldBe(execStr(/^q/, "abc"), "null");              // filtered no-match
    shouldBe(RegExp.lastMatch, savedLastMatch);
}

for (var i = 0; i < testLoopCount; ++i)
    step();
