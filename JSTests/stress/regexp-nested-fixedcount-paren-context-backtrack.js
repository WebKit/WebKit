//@ runDefault

// A fixed-count parenthesis that takes the ParenContext path would hang when
// its body itself contained another ParenContext-using term. restoreParenContext
// rewrites every nested frame slot, including the inner term's parenContextHead.
// The content-backtrack path then walked and freed that inner chain before the
// middle layer's Begin.bt re-restored the same pointer from its own ParenContext,
// at which point the pointer sat on the freelist and the skip loop followed it
// forever. FixedCount backtrack now reuses contexts in place instead of freeing
// them, so all outer snapshots stay valid.

function test(re, input, expected) {
    const actual = re.exec(input);
    const actualStr = actual === null ? "null" : JSON.stringify(Array.from(actual));
    const expectedStr = expected === null ? "null" : JSON.stringify(expected);
    if (actualStr !== expectedStr)
        throw new Error(`FAIL: ${re} on ${JSON.stringify(input)}: expected ${expectedStr}, got ${actualStr}`);
}

// Three nested capturing FixedCount — the original hang. Input lengths 5-7
// exercise the backtrack path where the outermost iteration partially
// succeeds before the innermost runs out of input.
test(/(((a){2}){2}){2}/, "aaaaa", null);
test(/(((a){2}){2}){2}/, "aaaaaa", null);
test(/(((a){2}){2}){2}/, "aaaaaaa", null);
test(/(((a){2}){2}){2}/, "aaaaaaaa", ["aaaaaaaa", "aaaa", "aa", "a"]);
test(/(((a){2}){2}){2}/, "aaaaaaaaa", ["aaaaaaaa", "aaaa", "aa", "a"]);

// Four levels.
test(/((((a){2}){2}){2}){2}/, "a".repeat(15), null);
test(/((((a){2}){2}){2}){2}/, "a".repeat(16), ["a".repeat(16), "a".repeat(8), "aaaa", "aa", "a"]);

// Variations on count and inner-term width.
test(/(((ab){2}){2}){2}/, "ababababab", null);
test(/(((ab){2}){2}){2}/, "abababababababab", ["abababababababab", "abababab", "abab", "ab"]);
test(/(((a){2}){3}){2}/, "a".repeat(11), null);
test(/(((a){2}){3}){2}/, "a".repeat(12), ["a".repeat(12), "a".repeat(6), "aa", "a"]);

// Named capture variant.
test(/(((?<n>a){2}){2}){2}/, "aaaaa", null);
test(/(((?<n>a){2}){2}){2}/, "aaaaaaaa", ["aaaaaaaa", "aaaa", "aa", "a"]);

// Backtrackable innermost content (greedy sub-term, multiple alternatives).
test(/(((a+){2}){2}){2}/, "aaaaaaa", null);
test(/(((a+){2}){2}){2}/, "aaaaaaaa", ["aaaaaaaa", "aaaa", "aa", "a"]);
test(/(((a|aa){2}){2}){2}/, "aaaaaaa", null);
test(/(((a|aa){2}){2}){2}/, "aaaaaaaa", ["aaaaaaaa", "aaaa", "aa", "a"]);
test(/((((a+){2}){2}){2}){2}/, "a".repeat(15), null);
test(/((((a+){2}){2}){2}){2}/, "a".repeat(16), ["a".repeat(16), "a".repeat(8), "aaaa", "aa", "a"]);

// Sibling FixedCount subtrees inside one outer FixedCount.
for (let len = 8; len <= 15; ++len)
    test(/((((a){2}){2})(((b){2}){2})){2}/, "aaaabbbbaaaabbbb".slice(0, len), null);
test(/((((a){2}){2})(((b){2}){2})){2}/, "aaaabbbbaaaabbbb", ["aaaabbbbaaaabbbb", "aaaabbbb", "aaaa", "aa", "a", "bbbb", "bb", "b"]);

// Inner Greedy / NonGreedy (also ParenContext-using) under an outer FixedCount.
test(/((a)+){2}/, "aaa", ["aaa", "a", "a"]);
test(/((a)*?b){2}/, "abab", ["abab", "ab", "a"]);
test(/((a)*?b){2}/, "abb", ["abb", "b", null]);

// Controls that were already passing — must keep passing.
test(/((a){2}){2}/, "aaa", null);
test(/((a){2}){2}/, "aaaa", ["aaaa", "aa", "a"]);
test(/(?:(?:(?:a){2}){2}){2}/, "aaaaa", null);
test(/(?:(?:(?:a){2}){2}){2}/, "aaaaaaaa", ["aaaaaaaa"]);
test(/(((?:a){2}){2}){2}/, "aaaaa", null);
test(/(((?:a){2}){2}){2}/, "aaaaaaaa", ["aaaaaaaa", "aaaa", "aa"]);
