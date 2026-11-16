// Regression test for https://bugs.webkit.org/show_bug.cgi?id=XXXXX
// Unicode backreference pattern character read offset was wrong when
// checkedOffset != inputPosition (i.e., when there are terms after the backreference).

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error("FAIL: " + message + ". Expected: " + JSON.stringify(expected) + ", Actual: " + JSON.stringify(actual));
}

// Warm up the JIT by running the tests many times.
for (var i = 0; i < 500; i++) {
    // Basic case: backreference followed by a literal character.
    // This triggers checkedOffset != inputPosition for the backreference term.
    var result = /(.)\1c/u.exec("\u{10000}\u{10000}c");
    shouldBe(result !== null, true, "/(.)\u005C1c/u should match surrogate pair followed by same pair and 'c'");
    shouldBe(result[0], "\u{10000}\u{10000}c", "/(.)\u005C1c/u matched string");
    shouldBe(result[1], "\u{10000}", "/(.)\u005C1c/u capture group");

    // Without trailing term (checkedOffset == inputPosition), this already worked.
    var result2 = /(.)\1/u.exec("\u{10000}\u{10000}");
    shouldBe(result2 !== null, true, "/(.)\u005C1/u should match");
    shouldBe(result2[0], "\u{10000}\u{10000}", "/(.)\u005C1/u matched string");

    // Multiple trailing terms.
    var result3 = /(.)\1cd/u.exec("\u{10000}\u{10000}cd");
    shouldBe(result3 !== null, true, "/(.)\u005C1cd/u should match");
    shouldBe(result3[0], "\u{10000}\u{10000}cd", "/(.)\u005C1cd/u matched string");

    // Case insensitive unicode backreference with trailing term.
    var result4 = /(.)\1c/iu.exec("aac");
    shouldBe(result4 !== null, true, "/(.)\u005C1c/iu should match 'aac'");
    shouldBe(result4[0], "aac", "/(.)\u005C1c/iu matched string");

    // Surrogate pair with case insensitive and trailing term.
    var result5 = /(.)\1c/iu.exec("\u{10000}\u{10000}c");
    shouldBe(result5 !== null, true, "/(.)\u005C1c/iu should match surrogate pair");
    shouldBe(result5[0], "\u{10000}\u{10000}c", "/(.)\u005C1c/iu matched string");
}
