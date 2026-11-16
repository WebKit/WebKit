// Multi-alt variable-count tests that exercise content-backtracking with
// alternative dispatch. If endOp.m_contentBacktrackEntryLabel were wrong for
// multi-alt, these would mismatch V8.

function check(re, input, expected, label) {
    const got = re.exec(input);
    const a = JSON.stringify(got);
    const e = JSON.stringify(expected);
    if (a !== e)
        throw new Error("FAIL " + label + ": expected " + e + " got " + a);
}

// Multi-alt with min > 0, content backtrack across alternatives.
// Iter k may have chosen alt A or alt B; content-bt must visit the correct one.

check(/(?:(?:aa|a)){3,4}b/, "aaab", ["aaab"], "multi-alt: aa|a x 3 then b");
check(/(?:(?:aa|a)){3,4}b/, "aaaab", ["aaaab"], "multi-alt: needs to mix aa and a");
check(/(?:(?:aa|a)){2,3}c/, "aaac", ["aaac"], "multi-alt success: aa+a+c or a+aa+c");

// Content-bt within alt forced by post-parens needing more chars.
check(/(?:(a+|b+)){2,4}xy/, "aabbxy", ["aabbxy", "bb"], "multi-alt: aa, bb, xy");
check(/(?:(a+|b+)){2,4}xy/, "aaaabbxy", ["aaaabbxy", "bb"], "multi-alt: aaaa, bb, xy");
check(/(?:(a+|b+)){2,4}bx/, "aaabx", ["aaabx", "a"], "post-parens 'bx' forces a+ to back off");

// Min > max-of-input — pure failure with multi-alt
check(/(?:(?:aa|a)){5,}b/, "aab", null, "multi-alt fail");
check(/(?:(a+|b+)){4,}c/, "abc", null, "multi-alt fail with mixed input");

// 3-way alternatives
check(/(?:(?:abc|ab|a)){2,3}/, "abcab", ["abcab"], "3-way alts");
check(/(?:(?:abc|ab|a)){2,3}x/, "abcabx", ["abcabx"], "3-way alts with anchor");

// Heavy multi-alt content-bt
check(/(?:(a|b)){2,4}c/, "abc", ["abc", "b"], "single-char alts, min=2");
check(/(?:(a|b)){3,5}c/, "ababc", ["ababc", "b"], "single-char alts, min=3");
check(/(?:(a|b)){4,6}c/, "ababc", ["ababc", "b"], "single-char alts (need 4)");

// Nested multi-alt with min > 0 outer and inner
check(/((?:a+|b+)){2,3}((?:c+|d+)){2,3}/, "aabbccdd", ["aabbccdd", "bb", "dd"], "nested multi-alt");
