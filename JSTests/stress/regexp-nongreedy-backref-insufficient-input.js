// Regression test for non-greedy backreference infinite loop when the
// capture is non-zero-width but there is not enough remaining input to
// match another repetition.
//
// In the JIT, the NonGreedy reentry code stored beginIndex AFTER
// checkNotEnoughInput. When checkNotEnoughInput failed (not enough
// input), beginIndex kept its stale value from a previous successful
// iteration. The backtrack code's zero-width progress check then saw
// index != beginIndex (stale) and allowed retry, looping forever.
//
// These patterns must all terminate promptly.

function shouldBe(actual, expected, description) {
    let actualStr = JSON.stringify(actual);
    let expectedStr = JSON.stringify(expected);
    if (actualStr !== expectedStr)
        throw new Error(description + ": expected " + expectedStr + ", got " + actualStr);
}

// ---- Core bug pattern: one successful match then insufficient input ----

// \1*? matches "ab" once (index 2→4), then can't match again (need 2
// chars but only 1 left). Without the fix the JIT loops forever here.
shouldBe(/(ab)\1*?X/.exec("ababY"), null,
    "one match then insufficient input, no X");

shouldBe(/(ab)\1*?X/.exec("abab"), null,
    "one match then insufficient input, string ends");

// ---- Longer capture ----

shouldBe(/(abc)\1*?X/.exec("abcabcY"), null,
    "longer capture, one match then insufficient input");

shouldBe(/(abcde)\1*?X/.exec("abcdeabcdeYY"), null,
    "5-char capture, one match then insufficient input");

// ---- Multiple successful matches before running out ----

shouldBe(/(ab)\1*?X/.exec("ababababY"), null,
    "three matches possible, then insufficient input");

// ---- Finite max count (should still terminate) ----

shouldBe(/(ab)\1{0,100}?X/.exec("ababY"), null,
    "finite max count, one match then insufficient input");

// ---- Case-insensitive ----

shouldBe(/(ab)\1*?X/i.exec("ababY"), null,
    "case-insensitive, one match then insufficient input");

shouldBe(/(AB)\1*?x/i.exec("ABABZ"), null,
    "case-insensitive upper, one match then insufficient input");

// ---- Named backreference ----

shouldBe(/(?<cap>ab)\k<cap>*?X/.exec("ababY"), null,
    "named backref, one match then insufficient input");

// ---- Correct matches still work ----

shouldBe(/(ab)\1*?X/.exec("ababX"), ["ababX", "ab"],
    "correct: one backref match then X");

shouldBe(/(ab)\1*?X/.exec("abX"), ["abX", "ab"],
    "correct: zero backref matches then X");

shouldBe(/(ab)\1*?X/.exec("abababX"), ["abababX", "ab"],
    "correct: two backref matches then X");

// ---- Zero-width captures still handled (original fix) ----

shouldBe(/()\1*?X/.exec("Y"), null,
    "empty capture, no match");

shouldBe(/(a)|\1*?X/.exec("Y"), null,
    "undefined capture in other alternative, no match");
