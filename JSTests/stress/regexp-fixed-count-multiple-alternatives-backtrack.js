// Regression test for fixed-count non-capturing parentheses with multiple alternatives.
// These patterns require backtracking across iterations, which the FixedCount JIT
// optimization does not support. They should fall back to the interpreter.

function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// Pattern: (?:aa|a){2}b
// Input: "aab"
// Correct: "a" + "a" + "b" = match "aab"
// Bug: "aa" consumed too much, second iteration fails, no backtrack to try "a"
shouldBe(/(?:aa|a){2}b/.exec("aab"), ["aab"], "(?:aa|a){2}b should match 'aab'");

// Pattern: (?:aaa|aa|a){3}b
// Input: "aaab"
// Correct: "a" + "a" + "a" + "b" = match "aaab"
shouldBe(/(?:aaa|aa|a){3}b/.exec("aaab"), ["aaab"], "(?:aaa|aa|a){3}b should match 'aaab'");

// More complex backtracking scenarios
shouldBe(/(?:ab|a){2}c/.exec("aac"), ["aac"], "(?:ab|a){2}c should match 'aac'");
shouldBe(/(?:abc|ab|a){3}d/.exec("abaad"), ["abaad"], "(?:abc|ab|a){3}d should match 'abaad'");

// Verify patterns still work in non-backtracking cases
shouldBe(/(?:aa|a){2}b/.exec("aaab"), ["aaab"], "(?:aa|a){2}b should match 'aaab'");
shouldBe(/(?:aa|a){2}/.exec("aaa"), ["aaa"], "(?:aa|a){2} should match 'aaa'");

// Verify single-alternative patterns still use JIT optimization (should be fast)
shouldBe(/(?:ab){3}/.exec("ababab"), ["ababab"], "(?:ab){3} should match 'ababab'");
shouldBe(/(?:a){5}b/.exec("aaaaab"), ["aaaaab"], "(?:a){5}b should match 'aaaaab'");

// Edge cases
shouldBe(/(?:aa|a){2}b/.exec("ab"), null, "(?:aa|a){2}b should not match 'ab'");
shouldBe(/(?:aa|a){3}b/.exec("aab"), null, "(?:aa|a){3}b should not match 'aab'");

print("All tests passed!");
