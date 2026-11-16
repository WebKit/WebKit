//@ runDefault("--useRegExpJIT=false")

// Regression test for tryConsumeBackReference pos corruption when a BMP
// captured character is compared against a surrogate pair in the input.
// readChecked() has a side effect of advancing pos by 1 when it decodes a
// surrogate pair.  When the comparison fails, uncheckInput only restores the
// checkInput amount, leaving pos off by 1.  This causes subsequent terms to
// read from the wrong position.

(function() {
    // /(a)\1*(.)/u  -- backreference \1 captures 'a' (BMP).
    // Input "a\u{10000}" -- after matching 'a', the greedy \1* attempts to
    // match another 'a' but finds U+10000 (surrogate pair).  The failed
    // tryConsumeBackReference should not corrupt pos, so (.) can still match
    // U+10000.
    var re1 = /(a)\1*(.)/u;
    var result1 = re1.exec("a\u{10000}");
    if (!result1)
        throw new Error("Test 1 failed: expected a match but got null");
    if (result1[0] !== "a\u{10000}")
        throw new Error("Test 1 failed: expected 'a\\u{10000}' but got '" + result1[0] + "'");
    if (result1[1] !== "a")
        throw new Error("Test 1 failed: capture 1 expected 'a' but got '" + result1[1] + "'");
    if (result1[2] !== "\u{10000}")
        throw new Error("Test 1 failed: capture 2 expected '\\u{10000}' but got '" + result1[2] + "'");

    // Same pattern with a different BMP character.
    var re2 = /(b)\1*(.)/u;
    var result2 = re2.exec("b\u{10000}");
    if (!result2)
        throw new Error("Test 2 failed: expected a match but got null");
    if (result2[0] !== "b\u{10000}")
        throw new Error("Test 2 failed: expected 'b\\u{10000}' but got '" + result2[0] + "'");
    if (result2[1] !== "b")
        throw new Error("Test 2 failed: capture 1 expected 'b' but got '" + result2[1] + "'");
    if (result2[2] !== "\u{10000}")
        throw new Error("Test 2 failed: capture 2 expected '\\u{10000}' but got '" + result2[2] + "'");

    // Verify that a backreference that *should* match still works.
    var re3 = /(a)\1/u;
    var result3 = re3.exec("aa");
    if (!result3)
        throw new Error("Test 3 failed: expected a match but got null");
    if (result3[0] !== "aa")
        throw new Error("Test 3 failed: expected 'aa' but got '" + result3[0] + "'");
})();
