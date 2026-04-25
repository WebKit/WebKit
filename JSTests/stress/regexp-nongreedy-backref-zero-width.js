// Regression test for non-greedy backreference infinite loop when the
// referenced capture is undefined or empty. The backtrack code for \N*?
// must detect that repeating zero-width matches cannot make progress and
// fail, rather than looping forever.
//
// These patterns must all terminate promptly. On buggy engines they hang
// because the non-greedy quantifier keeps retrying zero-width matches.

function shouldBe(actual, expected, description) {
    let actualStr = JSON.stringify(actual);
    let expectedStr = JSON.stringify(expected);
    if (actualStr !== expectedStr)
        throw new Error(description + ": expected " + expectedStr + ", got " + actualStr);
}

// ---- Backreference to capture in a different alternative (undefined) ----

// \1 refers to group 1 which is in alternative 1. When alternative 2 is
// tried, group 1 is undefined, so \1*? matches zero-width on every attempt.
shouldBe(/(a)|\1*?X/.exec("YYYYYY"), null,
    "backref to group in other alternative, no match");

shouldBe(/(a)|\1*?X/.exec("X"), ["X", undefined],
    "backref to group in other alternative, X matches");

shouldBe(/(a)|\1*?X/.exec("aYYYYY"), ["a", "a"],
    "backref to group in other alternative, alt 1 matches");

// ---- Backreference to empty capture ----

// Group 1 captures the empty string. \1*? repeatedly matches empty.
shouldBe(/()\1*?X/.exec("YYYYYY"), null,
    "backref to empty capture, no match");

shouldBe(/()\1*?X/.exec("X"), ["X", ""],
    "backref to empty capture, X matches");

// ---- Backreference to unmatched optional group ----

// (a)? may or may not capture. When it doesn't, \1 is undefined.
shouldBe(/(a)?\1*?X/.exec("YYYYYY"), null,
    "backref to unmatched optional group, no match");

shouldBe(/(a)?\1*?X/.exec("X"), ["X", undefined],
    "backref to unmatched optional group, X matches");

shouldBe(/(a)?\1*?X/.exec("aaX"), ["aaX", "a"],
    "backref to matched optional group, a then backref then X");

// ---- Named backreference to undefined capture in another alternative ----

shouldBe(/(?<t>A)|(?<t>\k<t>*?B)/.exec("BBBB"), ["B", undefined, "B"],
    "named backref to group in other alternative");

// ---- Multiple undefined backreferences with non-greedy quantifiers ----

shouldBe(/(a)(b)|\1*?\2*?X/.exec("YYYYYY"), null,
    "two undefined backrefs with non-greedy quantifiers");

shouldBe(/(a)(b)|\1*?\2*?X/.exec("X"), ["X", undefined, undefined],
    "two undefined backrefs, X matches");

// ---- Case-insensitive with undefined backref ----

shouldBe(/(a)|\1*?X/i.exec("yyyyyy"), null,
    "case-insensitive undefined backref, no match");

// ---- Non-greedy backref that does match (should still work correctly) ----

shouldBe(/^(.)\1*?(X)/.exec("======X"), ["======X", "=", "X"],
    "non-greedy backref to non-empty capture");

shouldBe(/^(.)\1*?(.+)/.exec("======="), ["=======", "=", "======"],
    "non-greedy backref greedy suffix");

// ---- The original crashing pattern from regexp-backreference-backtrack-interpreter.js ----

shouldBe(/A(B(\k<t>C)D(?<t>))EFGHIJKLM|(?<t>\k<t>*?B)B/.exec("aabbbccc"), null,
    "complex pattern with forward ref and non-greedy backref");

shouldBe(/A(B(\k<t>C)D(?<t>))EFGHIJKLM|(?<t>\k<t>*B)B/.exec("aabbbccc"), null,
    "complex pattern with greedy backref (should not hang)");

shouldBe(/A(B(\k<t>C)D(?<t>))EFGHIJKLM|(?<t>\k<t>*?B)B/.exec("aabbbccc"), null,
    "complex pattern with non-greedy variant");
