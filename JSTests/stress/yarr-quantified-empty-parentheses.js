// Coverage for quantified parentheses whose body can match an empty string —
// e.g. /(){3}/, /(?:){5}/, /((?:)){2}/, /(a?){3}/. The Yarr JIT punts these
// runs to the interpreter via the empty-match-detection branch in
// ParenthesesSubpattern[FixedCount]End; this test exercises both the JIT
// (which aborts and bails) and the interpreter to confirm they agree on
// the captured value, the match index, and overall match success.
// Expected values were verified against the Yarr interpreter and V8.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: got ${actual}, expected ${expected}`);
}

function match(re, str) {
    let m = re.exec(str);
    return m === null ? "null" : JSON.stringify(Array.from(m).map(x => x === undefined ? null : x)) + "@" + m.index;
}

function test(reSource, flags, str, expected) {
    let re = new RegExp(reSource, flags);
    for (let i = 0; i < 200; ++i)
        shouldBe(match(re, str), expected);
}

// --- Capturing FixedCount empty body ---
test("(){3}", "", "", '["",""]@0');
test("(){3}", "", "abc", '["",""]@0');
test("^(){3}$", "", "", '["",""]@0');
test("^(){3}$", "", "a", "null");
test("a(){3}", "", "a", '["a",""]@0');
test("a(){3}", "", "abc", '["a",""]@0');
test("a(){3}b", "", "ab", '["ab",""]@0');
test("(){1}", "", "x", '["",""]@0');
test("(){10}", "", "x", '["",""]@0');

// --- Non-capturing FixedCount empty body ---
test("(?:){3}", "", "abc", '[""]@0');
test("^(?:){3}$", "", "", '[""]@0');
test("a(?:){3}b", "", "ab", '["ab"]@0');

// --- Capturing empty alternative inside ---
test("((?:)){3}", "", "abc", '["",""]@0');
test("((?:)){2}x", "", "x", '["x",""]@0');

// --- Mixed: outer non-empty, inner can be empty ---
test("(a()){3}", "", "aaa", '["aaa","a",""]@0');
test("(a()){3}b", "", "aaab", '["aaab","a",""]@0');
test("((a)()){2}", "", "aa", '["aa","a","a",""]@0');

// --- Optional content (a? matches empty when a is absent) ---
test("(a?){3}", "", "", '["",""]@0');
test("(a?){3}", "", "a", '["a",""]@0');
test("(a?){3}", "", "aaa", '["aaa","a"]@0');
test("(a?){3}", "", "aaaa", '["aaa","a"]@0');
test("^(a?){3}$", "", "", '["",""]@0');
test("^(a?){3}$", "", "aaa", '["aaa","a"]@0');
test("^(a?){3}$", "", "aaaa", "null");

// --- Empty alternation ---
test("(|x){3}", "", "", '["",""]@0');
test("(|x){3}", "", "xxx", '["",""]@0');
test("(x|){3}", "", "xx", '["xx",""]@0');

// --- Quantified empty paren followed by capturing content ---
test("(){2}(a)", "", "a", '["a","","a"]@0');
test("(){2}(a)b", "", "ab", '["ab","","a"]@0');

// --- Greedy empty (?: ... )* — JIT also bails for the empty case ---
test("(?:)*", "", "abc", '[""]@0');
test("()*", "", "abc", '["",null]@0');
test("()+", "", "abc", '["",""]@0');

// --- Backreference to an empty-body capture group ---
test("(){3}\\1", "", "abc", '["",""]@0');
test("()\\1", "", "abc", '["",""]@0');
