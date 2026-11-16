function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected " + expected);
}

function stringify(match) {
    if (match === null)
        return "null";
    return "[" + Array.from(match, (x) => x === undefined ? "undefined" : JSON.stringify(x)).join(",") + "]";
}

function stringifyIndices(match) {
    if (match === null)
        return "null";
    return JSON.stringify(Array.from(match.indices, (x) => x === undefined ? null : x));
}

shouldBe(stringify(/(?<=(?:ab)+)c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:ab)+)c/.exec("abc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:ab)+)c/.exec("c")), "null");
shouldBe(stringify(/(?<=(?:ab)+)c/.exec("xabc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:ab)+)c/.exec("aabc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab)+)c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab)+)c/.exec("xababc")), "null");
shouldBe(stringify(/(?<=(?:ab)*)c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:ab)*)c/.exec("c")), "[\"c\"]");
shouldBe(stringify(/(?<=x(?:ab)*)c/.exec("xababc")), "[\"c\"]");
shouldBe(stringify(/(?<=x(?:ab)*)c/.exec("xc")), "[\"c\"]");
shouldBe(stringify(/(?<=x(?:ab)*)c/.exec("ababc")), "null");
shouldBe(stringify(/(?<=(ab)+)c/.exec("ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=(ab)+)c/.exec("abc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=(ab)*)c/.exec("c")), "[\"c\",undefined]");
shouldBe(stringify(/(?<=(a)+)b/.exec("aaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(a)*)b/.exec("aaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(a)*)b/.exec("b")), "[\"b\",undefined]");
shouldBe(stringify(/(?<=(a)+)b/.exec("b")), "null");
shouldBe(stringify(/(?<=^(a)+)b/.exec("aaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=^(a)+)b/.exec("xaaab")), "null");
shouldBe(stringify(/(?<=^(a)*)b/.exec("xaaab")), "null");
shouldBe(stringify(/(?<=(a|b)+)c/.exec("abbac")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=(a|b)+)c/.exec("c")), "null");
shouldBe(stringify(/(?<=(a|bb)+)c/.exec("abbac")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=(a|bb)+)c/.exec("bbc")), "[\"c\",\"bb\"]");
shouldBe(stringify(/(?<=(a|bb)+)c/.exec("bc")), "null");
shouldBe(stringify(/(?<=^(a|ab)+)c/.exec("aabc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=^(a|ab)+)c/.exec("ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=^(ab|a)+)c/.exec("aabc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=^(ab|a)+)c/.exec("abac")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=^(?:a|ab)+)c/.exec("abaabc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab|a)+)c/.exec("abaabc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:a|b)*)c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=x(?:a|b)*)c/.exec("xababc")), "[\"c\"]");
shouldBe(stringify(/(?<=x(?:a|b)*)c/.exec("xc")), "[\"c\"]");
shouldBe(stringify(/(?<=x(?:a|b)*)c/.exec("yababc")), "null");
shouldBe(stringify(/(?<=(a|b)*)c/.exec("ababc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=(a|b)*)c/.exec("abbc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=(?:ab)+?)c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:ab)+?)c/.exec("c")), "null");
shouldBe(stringify(/(?<=^(?:ab)+?)c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab)+?)c/.exec("xababc")), "null");
shouldBe(stringify(/(?<=^(?:a)+?)b/.exec("aaab")), "[\"b\"]");
shouldBe(stringify(/(?<=^(?:a)+?)b/.exec("ab")), "[\"b\"]");
shouldBe(stringify(/(?<=^(?:a)+?)b/.exec("b")), "null");
shouldBe(stringify(/(?<=^(?:a)*?)b/.exec("aaab")), "[\"b\"]");
shouldBe(stringify(/(?<=^(?:a)*?)b/.exec("b")), "[\"b\"]");
shouldBe(stringify(/(?<=x(?:a)*?)b/.exec("xaaab")), "[\"b\"]");
shouldBe(stringify(/(?<=x(?:a)*?)b/.exec("aaab")), "null");
shouldBe(stringify(/(?<=(a)+?)b/.exec("aaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(a)*?)b/.exec("aaab")), "[\"b\",undefined]");
shouldBe(stringify(/(?<=^(a)+?)b/.exec("aaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=^(a)*?)b/.exec("aaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=^(a|b)+?)c/.exec("abbac")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=^(a|bb)+?)c/.exec("abbac")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=^(a|bb)*?)c/.exec("abbac")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=^(a|bb)*?)c/.exec("c")), "[\"c\",undefined]");
shouldBe(stringify(/(?<=^(?:a|ab)+?)c/.exec("abaabc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab|a)+?)c/.exec("abaabc")), "[\"c\"]");
shouldBe(stringify(/(?<=(a|ab){1,2}?c)d/.exec("abacd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=(a|ab){1,2}?c)d/.exec("aabcd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=^(a|ab){1,2}?c)d/.exec("abacd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=^(a|ab){1,2}?c)d/.exec("aabcd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=^(a|ab){1,2}?c)d/.exec("abcd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=^(a|ab){1,2}?c)d/.exec("acd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=y(yb|b){1,2}?c)d/.exec("xybcd")), "[\"d\",\"b\"]");
shouldBe(stringify(/(?<=y(yb|b){1,3}?c)d/.exec("xybcd")), "[\"d\",\"b\"]");
shouldBe(stringify(/(?<=y(yb|b){1,}?c)d/.exec("xybcd")), "[\"d\",\"b\"]");
shouldBe(stringify(/(?<=y(yb|b){1,2}c)d/.exec("xybcd")), "[\"d\",\"b\"]");
shouldBe(stringify(/(?<=y(yb|b){1,3}c)d/.exec("xybcd")), "[\"d\",\"b\"]");
shouldBe(stringify(/(?<=y(yb|b){1,}c)d/.exec("xybcd")), "[\"d\",\"b\"]");
shouldBe(stringify(/(?<=y(yb|b){2,3}?c)d/.exec("xybbcd")), "[\"d\",\"b\"]");
shouldBe(stringify(/(?<=y(yb|b){2,3}c)d/.exec("xybbcd")), "[\"d\",\"b\"]");
shouldBe(stringify(/(?<=y(?:(yb)|(b)){1,2}?c)d/.exec("xybcd")), "[\"d\",undefined,\"b\"]");
shouldBe(stringify(/(?<=y(?<g>yb|b){1,2}?c)d/.exec("xybcd")), "[\"d\",\"b\"]");
shouldBe(stringify(/(?<=y(yb|b){1,2}?c)d\1/.exec("xybcdb")), "[\"db\",\"b\"]");
shouldBe(stringify(/(?<=y(yb|b){1,2}?c)d\1/.exec("xybcdyb")), "null");
shouldBe(stringify(/(?<=(?:ab){2})c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:ab){2})c/.exec("abc")), "null");
shouldBe(stringify(/(?<=(?:ab){2})c/.exec("abababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab){2})c/.exec("abababc")), "null");
shouldBe(stringify(/(?<=^(?:ab){3})c/.exec("abababc")), "[\"c\"]");
shouldBe(stringify(/(?<=(ab){2})c/.exec("ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=(ab){2})c/.exec("abc")), "null");
shouldBe(stringify(/(?<=(a){3})b/.exec("aaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(a){3})b/.exec("aab")), "null");
shouldBe(stringify(/(?<=x(a){3})b/.exec("xaaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=x(a){3})b/.exec("aaaab")), "null");
shouldBe(stringify(/(?<=([ab]){2})c/.exec("abc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=([ab]){2})c/.exec("bac")), "[\"c\",\"b\"]");
shouldBe(stringify(/(?<=([ab]){2})c/.exec("ac")), "null");
shouldBe(stringify(/(?<=(\d{2},){2})x/.exec("12,34,x")), "[\"x\",\"12,\"]");
shouldBe(stringify(/(?<=(\d{2},){2})x/.exec("12,3,x")), "null");
shouldBe(stringify(/(?<=(?:a|ab){2})c/.exec("aabc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:a|ab){2})c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:a|ab){2})c/.exec("abc")), "null");
shouldBe(stringify(/(?<=^(?:a|ab){2})c/.exec("aabc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:a|ab){2})c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:a|ab){2})c/.exec("abac")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:a|ab){2})c/.exec("abc")), "null");
shouldBe(stringify(/(?<=^(?:ab|a){2})c/.exec("aabc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab|a){2})c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab|a){2})c/.exec("abac")), "[\"c\"]");
shouldBe(stringify(/(?<=(a|ab){2})c/.exec("aabc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=(a|ab){2})c/.exec("ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=^(a|ab){2})c/.exec("ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=^(a|ab){2})c/.exec("abac")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=^(ab|a){2})c/.exec("abac")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=^(ab|a){2})c/.exec("aabc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=(?:a+){2})b/.exec("aab")), "[\"b\"]");
shouldBe(stringify(/(?<=(?:a+){2})b/.exec("ab")), "null");
shouldBe(stringify(/(?<=(?:a+){2})b/.exec("aaaab")), "[\"b\"]");
shouldBe(stringify(/(?<=^(?:a+){2})b/.exec("aaaab")), "[\"b\"]");
shouldBe(stringify(/(?<=^(?:a+?){2})b/.exec("aaaab")), "[\"b\"]");
shouldBe(stringify(/(?<=^(a+){2})b/.exec("aaaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=^(a+?){2})b/.exec("aaaab")), "[\"b\",\"aaa\"]");
shouldBe(stringify(/(?<=^(a*){2})b/.exec("aaaab")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=^(a*){2})b/.exec("b")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=(a*?){2})b/.exec("aaaab")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=^(a\d*){2})b/.exec("a12a3b")), "[\"b\",\"a12\"]");
shouldBe(stringify(/(?<=^(a\d*){2})b/.exec("a12ab")), "[\"b\",\"a12\"]");
shouldBe(stringify(/(?<=^(a\d*){2})b/.exec("a123b")), "null");
shouldBe(stringify(/(?<=(?:ab){1,2})c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:ab){1,2})c/.exec("abc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:ab){1,2})c/.exec("c")), "null");
shouldBe(stringify(/(?<=^(?:ab){1,2})c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab){1,2})c/.exec("abababc")), "null");
shouldBe(stringify(/(?<=^(?:ab){2,3})c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab){2,3})c/.exec("abababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab){2,3})c/.exec("abc")), "null");
shouldBe(stringify(/(?<=^(?:ab){2,3})c/.exec("ababababc")), "null");
shouldBe(stringify(/(?<=^(?:ab){2,})c/.exec("ababababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab){2,})c/.exec("abc")), "null");
shouldBe(stringify(/(?<=^(?:ab){0,2})c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab){0,2})c/.exec("c")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:ab){0,2})c/.exec("abababc")), "null");
shouldBe(stringify(/(?<=(ab){1,2})c/.exec("ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=(ab){2,3})c/.exec("abababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=(ab){2,3})c/.exec("ababababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=(ab){2,3})c/.exec("abc")), "null");
shouldBe(stringify(/(?<=(ab){2,})c/.exec("ababababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=(ab){0,2})c/.exec("c")), "[\"c\",undefined]");
shouldBe(stringify(/(?<=(ab){0,2})c/.exec("ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=^(a|ab){1,2})c/.exec("aabc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=^(a|ab){1,2})c/.exec("ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=^(a|ab){2,3})c/.exec("aababc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=^(a|ab){2,3})c/.exec("abababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=^(a|ab){2,3})c/.exec("ac")), "null");
shouldBe(stringify(/(?<=^(?:a|ab){2,3})c/.exec("aababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:a|ab){2,3})c/.exec("abababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:a|ab){2,3}?)c/.exec("aababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:a|ab){2,3}?)c/.exec("abababc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(a|ab){2,3}?)c/.exec("aababc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=^(a|ab){1,2}?)c/.exec("ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=^(a|ab){1,}?)c/.exec("ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=^(a|ab){1,})c/.exec("ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=(\d{1,3},)*\d+)x/.exec("1,22,333x")), "[\"x\",\"1,\"]");
shouldBe(stringify(/(?<=(\d{1,3},)*\d+)x/.exec("1x")), "[\"x\",undefined]");
shouldBe(stringify(/(?<=(\d{1,3},)*\d+)x/.exec("x")), "null");
shouldBe(stringify(/(?<=^(\d{1,3},)*\d+)x/.exec("1,22,333x")), "[\"x\",\"1,\"]");
shouldBe(stringify(/(?<=^(\d{1,3},)*\d+)x/.exec("1,2222,333x")), "null");
shouldBe(stringify(/(?<=^(\d{1,3},)*\d{1,3})x/.exec("1,2222,333x")), "null");
shouldBe(stringify(/(?<=^(\d{1,3},)+\d+)x/.exec("1x")), "null");
shouldBe(stringify(/(?<=^(\d{1,3},)+\d+)x/.exec("1,2x")), "[\"x\",\"1,\"]");
shouldBe(stringify(/(?<=(?:(?:ab)+c)+)d/.exec("ababcabcd")), "[\"d\"]");
shouldBe(stringify(/(?<=(?:(?:ab)+c)+)d/.exec("cd")), "null");
shouldBe(stringify(/(?<=(?:(?:ab)+c)+)d/.exec("d")), "null");
shouldBe(stringify(/(?<=^(?:(?:ab)+c)+)d/.exec("ababcabcd")), "[\"d\"]");
shouldBe(stringify(/(?<=^(?:(?:ab)+c)+)d/.exec("xababcabcd")), "null");
shouldBe(stringify(/(?<=^((ab)+c)+)d/.exec("ababcabcd")), "[\"d\",\"ababc\",\"ab\"]");
shouldBe(stringify(/(?<=^((ab)+c)+)d/.exec("abcababcd")), "[\"d\",\"abc\",\"ab\"]");
shouldBe(stringify(/(?<=^((ab)+?c)+?)d/.exec("abcababcd")), "[\"d\",\"abc\",\"ab\"]");
shouldBe(stringify(/(?<=^((?:ab){2}c){2})d/.exec("ababcababcd")), "[\"d\",\"ababc\"]");
shouldBe(stringify(/(?<=^((?:ab){2}c){2})d/.exec("ababcabcd")), "null");
shouldBe(stringify(/(?<=^(?:(a|b){2}c)+)d/.exec("abcbacd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=^(?:(a|b){2}c)+)d/.exec("abcbcd")), "null");
shouldBe(stringify(/(?<=^(?:(a|b)+c){2})d/.exec("abcbacd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=^(?:(a|b)+c){2})d/.exec("abcd")), "null");
shouldBe(stringify(/(?<=(?:(?:a|b)+c){2,3})d/.exec("acbcabcd")), "[\"d\"]");
shouldBe(stringify(/(?<=^(?:(?:a|b)+c){2,3})d/.exec("acbcabcd")), "[\"d\"]");
shouldBe(stringify(/(?<=^(?:(?:a|b)+c){2,3})d/.exec("acbcabcbcd")), "null");
shouldBe(stringify(/(?<=^(?:(?:a|b)+?c){2,3}?)d/.exec("acbcabcd")), "[\"d\"]");
shouldBe(stringify(/(?<=(?:a){1,2}(?:b){1,2})c/.exec("aabbc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:a){1,2}(?:b){1,2})c/.exec("abc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:a){1,2}(?:b){1,2})c/.exec("ac")), "null");
shouldBe(stringify(/(?<=(?:a){1,2}(?:b){1,2})c/.exec("bbc")), "null");
shouldBe(stringify(/(?<=^(?:a){1,2}(?:b){1,2})c/.exec("aabbc")), "[\"c\"]");
shouldBe(stringify(/(?<=^(?:a){1,2}(?:b){1,2})c/.exec("aaabbc")), "null");
shouldBe(stringify(/(?<=^(?:a){1,2}(?:b){1,2})c/.exec("aabbbc")), "null");
shouldBe(stringify(/(?<=^(a){1,2}(b){1,2})c/.exec("aabbc")), "[\"c\",\"a\",\"b\"]");
shouldBe(stringify(/(?<=^(a){1,2}(b){1,2})c/.exec("abc")), "[\"c\",\"a\",\"b\"]");
shouldBe(stringify(/(?<=^(a){1,2}?(b){1,2}?)c/.exec("aabbc")), "[\"c\",\"a\",\"b\"]");
shouldBe(stringify(/(?<=^(a){2,}(b){2,})c/.exec("aaabbbc")), "[\"c\",\"a\",\"b\"]");
shouldBe(stringify(/(?<=^(a){2,}(b){2,})c/.exec("aabc")), "null");
shouldBe(stringify(/(?<=^(a|b){1,3}(c|d){1,3})e/.exec("abcde")), "[\"e\",\"a\",\"c\"]");
shouldBe(stringify(/(?<=^(a|b){1,3}(c|d){1,3})e/.exec("abbcdde")), "[\"e\",\"a\",\"c\"]");
shouldBe(stringify(/(?<=^(a|b){1,3}(c|d){1,3})e/.exec("abbbcde")), "null");
shouldBe(stringify(/(?<=^(?:a|b){1,3}(?:c|d){1,3})e/.exec("abcde")), "[\"e\"]");
shouldBe(stringify(/(?<=^(?:a|b){1,3}(?:c|d){1,3}?)e/.exec("abcde")), "[\"e\"]");
shouldBe(stringify(/(?<=(?:ab)+(?:cd)+)e/.exec("ababcdcde")), "[\"e\"]");
shouldBe(stringify(/(?<=(?:ab)+(?:cd)+)e/.exec("abcde")), "[\"e\"]");
shouldBe(stringify(/(?<=(?:ab)+(?:cd)+)e/.exec("abe")), "null");
shouldBe(stringify(/(?<=^(?:ab)+(?:cd)+)e/.exec("ababcdcde")), "[\"e\"]");
shouldBe(stringify(/(?<=^(?:ab)+(?:cd)+)e/.exec("xababcdcde")), "null");
shouldBe(stringify(/(?<=^(?:ab)+?(?:cd)+?)e/.exec("ababcdcde")), "[\"e\"]");
shouldBe(stringify(/(?<=^(ab)+(cd)+)e/.exec("ababcdcde")), "[\"e\",\"ab\",\"cd\"]");
shouldBe(stringify(/(?<=^(ab)+?(cd)+?)e/.exec("ababcdcde")), "[\"e\",\"ab\",\"cd\"]");
shouldBe(stringify(/(?<=^(?:ab){2}(?:cd){2})e/.exec("ababcdcde")), "[\"e\"]");
shouldBe(stringify(/(?<=^(?:ab){2}(?:cd){2})e/.exec("abcdcde")), "null");
shouldBe(stringify(/(?<=^(ab){2}(cd){2})e/.exec("ababcdcde")), "[\"e\",\"ab\",\"cd\"]");
shouldBe(stringify(/(?<=\b(?:ab)+)c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=\b(?:ab)+)c/.exec("xababc")), "null");
shouldBe(stringify(/(?<=\b(?:ab)+)c/.exec("x ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=\b(ab)+)c/.exec("x ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=\b(a|ab)+)c/.exec("x aabc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=\B(?:ab)+)c/.exec("xababc")), "[\"c\"]");
shouldBe(stringify(/(?<=\B(?:ab)+)c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:^|,)(?:ab)+)c/.exec("ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:^|,)(?:ab)+)c/.exec("x,ababc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:^|,)(?:ab)+)c/.exec("xababc")), "null");
shouldBe(stringify(/(?<=(?:^|,)(ab){2})c/.exec("x,ababc")), "[\"c\",\"ab\"]");
shouldBe(stringify(/(?<=(?:^|,)(ab){2})c/.exec("x,abababc")), "null");
shouldBe(stringify(/(?<=^(?:ab)+$)/.exec("abab")), "[\"\"]");
shouldBe(stringify(/(?<=^(?:ab)+$)/.exec("aba")), "null");
shouldBe(stringify(/(?<=^(ab)+$)/.exec("abab")), "[\"\",\"ab\"]");
shouldBe(stringify(/(?<=^(a|ab)+$)/.exec("abaab")), "[\"\",\"ab\"]");
shouldBe(stringify(/(?<=^(?:ab)+)$/m.exec("ab\nabab")), "[\"\"]");
shouldBe(stringify(/(?<=^(ab)+)$/m.exec("ab\nabab")), "[\"\",\"ab\"]");
shouldBe(stringify(/(?<=^(ab)+)$/m.exec("ab\nabab\nx")), "[\"\",\"ab\"]");
shouldBe(stringify(/(?<=(?:ab)+c)d/.exec("ababcd")), "[\"d\"]");
shouldBe(stringify(/(?<=(?:ab)+c)d/.exec("cd")), "null");
shouldBe(stringify(/(?<=(?:ab)+c)d/.exec("abcd")), "[\"d\"]");
shouldBe(stringify(/(?<=(ab)+c)d/.exec("ababcd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=(a|b)+c)d/.exec("abbacd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=(a|b)+c)d/.exec("cd")), "null");
shouldBe(stringify(/(?<=(a|b)*c)d/.exec("cd")), "[\"d\",undefined]");
shouldBe(stringify(/(?<=(a|b)*c)d/.exec("abcd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=(?:ab){2}c)d/.exec("ababcd")), "[\"d\"]");
shouldBe(stringify(/(?<=(?:ab){2}c)d/.exec("abcd")), "null");
shouldBe(stringify(/(?<=(ab){2}c)d/.exec("ababcd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=(a|ab){2}c)d/.exec("ababcd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=(a|ab){2}c)d/.exec("aabcd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=(a|ab){2}c)d/.exec("abacd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=(a|ab){1,2}c)d/.exec("abacd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=(a|ab){1,2}c)d/.exec("acd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=(a|ab){1,2}c)d/.exec("cd")), "null");
shouldBe(stringify(/(?<=x(?:ab)+c)d/.exec("xababcd")), "[\"d\"]");
shouldBe(stringify(/(?<=x(?:ab)+c)d/.exec("xcd")), "null");
shouldBe(stringify(/(?<=x(ab)+c)d/.exec("xababcd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=x(a|b)+c)d/.exec("xabbacd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=x(?:ab)+?c)d/.exec("xababcd")), "[\"d\"]");
shouldBe(stringify(/(?<=x(ab)+?c)d/.exec("xababcd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=x(a|b)+?c)d/.exec("xabbacd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=x(a|ab)+?c)d/.exec("xabaabcd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=x(?:a|ab)+?c)d/.exec("xabaabcd")), "[\"d\"]");
shouldBe(stringify(/(?<=x(?:a|ab)+c)d/.exec("xabaabcd")), "[\"d\"]");
shouldBe(stringify(/(?<=x(a|ab)+c)d/.exec("xabaabcd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=x(ab|a)+c)d/.exec("xabaabcd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=x(a|ab){2}c)d/.exec("xabacd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=x(ab|a){2}c)d/.exec("xabacd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=x(a|ab){2}c)d/.exec("xaabcd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=x(ab|a){2}c)d/.exec("xaabcd")), "[\"d\",\"a\"]");
shouldBe(stringify(/(?<=x(a|ab){2}c)d/.exec("xababcd")), "[\"d\",\"ab\"]");
shouldBe(stringify(/(?<=x(a|ab){2}c)d/.exec("xabcd")), "null");
shouldBe(stringify(/(?<!(?:ab)+)c/.exec("ababc")), "null");
shouldBe(stringify(/(?<!(?:ab)+)c/.exec("xc")), "[\"c\"]");
shouldBe(stringify(/(?<!(?:ab)+)c/.exec("c")), "[\"c\"]");
shouldBe(stringify(/(?<!(?:ab){2})c/.exec("ababc")), "null");
shouldBe(stringify(/(?<!(?:ab){2})c/.exec("abc")), "[\"c\"]");
shouldBe(stringify(/(?<!(ab){2})c/.exec("abc")), "[\"c\",undefined]");
shouldBe(stringify(/(?<!(a|ab){2})c/.exec("aabc")), "null");
shouldBe(stringify(/(?<!(a|ab){2})c/.exec("bc")), "[\"c\",undefined]");
shouldBe(stringify(/(?<!^(a|ab){2})c/.exec("abac")), "null");
shouldBe(stringify(/(?<!^(a|ab){2})c/.exec("xabac")), "[\"c\",undefined]");
shouldBe(stringify(/(?<!(?:ab)+?)c/.exec("ababc")), "null");
shouldBe(stringify(/(?<!(?:ab)+?)c/.exec("xc")), "[\"c\"]");
shouldBe(stringify(/(?<!(\d{1,3},)*\d+)x/.exec("1,22,333x")), "null");
shouldBe(stringify(/(?<!(\d{1,3},)*\d+)x/.exec("ax")), "[\"x\",undefined]");
shouldBe(stringify(/(?<=(ab)+)c\1/.exec("ababcab")), "[\"cab\",\"ab\"]");
shouldBe(stringify(/(?<=(ab)+)c\1/.exec("ababcba")), "null");
shouldBe(stringify(/(?<=(a|b)+)c\1/.exec("abcb")), "null");
shouldBe(stringify(/(?<=(a|b)+)c\1/.exec("abca")), "[\"ca\",\"a\"]");
shouldBe(stringify(/(?<=(a|ab){2})c\1/.exec("aabcab")), "[\"ca\",\"a\"]");
shouldBe(stringify(/(?<=(a|ab){2})c\1/.exec("aabca")), "[\"ca\",\"a\"]");
shouldBe(stringify(/(?<=(ab){2})c\1/.exec("ababcab")), "[\"cab\",\"ab\"]");
shouldBe(stringify(/(?<=(?<x>ab)+)c\k<x>/.exec("ababcab")), "[\"cab\",\"ab\"]");
shouldBe(stringify(/(?<=(?<x>a|b)+)c\k<x>/.exec("abcb")), "null");
shouldBe(stringify(/(?<=(a)\1+)b/.exec("aaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=^(a)\1+)b/.exec("aaab")), "null");
shouldBe(stringify(/(?<=^(a)\1{2})b/.exec("aaab")), "null");
shouldBe(stringify(/(?<=^(a)\1{2})b/.exec("aab")), "null");
shouldBe(stringify(/(?<=^(?:(a)\1)+)b/.exec("aaaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=^(?:(a)\1)+)b/.exec("aaab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=^(?:(a|b)\1)+)c/.exec("aabbc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=^(?:(a|b)\1)+)c/.exec("aabc")), "[\"c\",\"a\"]");
shouldBe(stringify(/(?<=^(?:(a|b)\1){2})c/.exec("aabbc")), "null");
shouldBe(stringify(/(?<=^(?:(a|b)\1){2})c/.exec("bbc")), "[\"c\",\"b\"]");
shouldBe(stringify(/(?<=(?:(a|b)\1){2})c/.exec("xaabbc")), "[\"c\",\"b\"]");
shouldBe(stringify(/(?<=(?:ab)+)c/i.exec("aBAbC")), "[\"C\"]");
shouldBe(stringify(/(?<=(?:ab){2})c/i.exec("ABabc")), "[\"c\"]");
shouldBe(stringify(/(?<=(?:ab){2})c/i.exec("ABc")), "null");
shouldBe(stringify(/(?<=(a|ab){2})c/i.exec("AaBc")), "[\"c\",\"A\"]");
shouldBe(stringify(/(?<=^(a|ab){1,2}?)c/i.exec("AABC")), "[\"C\",\"A\"]");
shouldBe(stringify(/(?<=(\w{1,3},)*\w+)x/i.exec("A,bb,CCCx")), "[\"x\",\"A,\"]");
shouldBe(stringify(/(?<=(?:a*)+)b/.exec("aab")), "[\"b\"]");
shouldBe(stringify(/(?<=(?:a*)+)b/.exec("b")), "[\"b\"]");
shouldBe(stringify(/(?<=(a*)+)b/.exec("aab")), "[\"b\",\"aa\"]");
shouldBe(stringify(/(?<=(a*)+)b/.exec("b")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=(a*)*)b/.exec("aab")), "[\"b\",\"aa\"]");
shouldBe(stringify(/(?<=(a*)*)b/.exec("b")), "[\"b\",undefined]");
shouldBe(stringify(/(?<=(a*){2})b/.exec("aab")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=(a*){2})b/.exec("b")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=(a?){2})b/.exec("aab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(a?){2})b/.exec("ab")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=(a?){2})b/.exec("b")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=(a?){2,3})b/.exec("ab")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=(?:a|){2})b/.exec("ab")), "[\"b\"]");
shouldBe(stringify(/(?<=(?:a|){2})b/.exec("b")), "[\"b\"]");
shouldBe(stringify(/(?<=(a|){2})b/.exec("ab")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=(a|){2})b/.exec("aab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(a|){2})b/.exec("b")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=^(a|){2})b/.exec("ab")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=^(a|){2})b/.exec("aab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=^(a|){2})b/.exec("aaab")), "null");
shouldBe(stringify(/(?<=(|a){2})b/.exec("aab")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=^(|a){2})b/.exec("aab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(a|)+)b/.exec("aab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(a|)+?)b/.exec("aab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(a|)*)b/.exec("aab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(?:a|)+)b/.exec("aab")), "[\"b\"]");
shouldBe(stringify(/(?<=(?:a|)+?)b/.exec("aab")), "[\"b\"]");
shouldBe(stringify(/(?<=(?:\b|a)+)b/.exec("aab")), "[\"b\"]");
shouldBe(stringify(/(?<=(a|\b)+)b/.exec("aab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(a|\b)*)b/.exec("aab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(?:a|\b){2})b/.exec("aab")), "[\"b\"]");
shouldBe(stringify(/(?<=(a|\b){2})b/.exec("ab")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=(a|\b){2})b/.exec("b")), "[\"b\",\"\"]");
shouldBe(stringify(/(?<=(\b|a){2})b/.exec("aab")), "[\"b\",\"a\"]");
shouldBe(stringify(/(?<=(\b|a){2})b/.exec("ab")), "[\"b\",\"\"]");

{
    let re = /(?<=(?:ab)+)c/g;
    re.lastIndex = 0;
    let match = re.exec("ababcabcc");
    shouldBe(match === null ? "null" : match.index, 4);
    shouldBe(re.lastIndex, 5);
}
{
    let re = /(?<=(?:ab)+)c/g;
    re.lastIndex = 3;
    let match = re.exec("ababcabcc");
    shouldBe(match === null ? "null" : match.index, 4);
    shouldBe(re.lastIndex, 5);
}
{
    let re = /(?<=(?:ab)+)c/g;
    re.lastIndex = 6;
    let match = re.exec("ababcabcc");
    shouldBe(match === null ? "null" : match.index, 7);
    shouldBe(re.lastIndex, 8);
}
{
    let re = /(?<=(?:ab)+)c/y;
    re.lastIndex = 0;
    let match = re.exec("ababcabcc");
    shouldBe(match === null ? "null" : match.index, "null");
    shouldBe(re.lastIndex, 0);
}
{
    let re = /(?<=(?:ab)+)c/y;
    re.lastIndex = 4;
    let match = re.exec("ababcabcc");
    shouldBe(match === null ? "null" : match.index, 4);
    shouldBe(re.lastIndex, 5);
}
{
    let re = /(?<=(?:ab)+)c/y;
    re.lastIndex = 5;
    let match = re.exec("ababcabcc");
    shouldBe(match === null ? "null" : match.index, "null");
    shouldBe(re.lastIndex, 0);
}
{
    let re = /(?<=(?:ab)+)c/y;
    re.lastIndex = 8;
    let match = re.exec("ababcabcc");
    shouldBe(match === null ? "null" : match.index, "null");
    shouldBe(re.lastIndex, 0);
}
{
    let re = /(?<=(ab){2})c/g;
    re.lastIndex = 0;
    let match = re.exec("ababcababcc");
    shouldBe(match === null ? "null" : match.index, 4);
    shouldBe(re.lastIndex, 5);
}
{
    let re = /(?<=(ab){2})c/g;
    re.lastIndex = 2;
    let match = re.exec("ababcababcc");
    shouldBe(match === null ? "null" : match.index, 4);
    shouldBe(re.lastIndex, 5);
}
{
    let re = /(?<=(ab){2})c/g;
    re.lastIndex = 6;
    let match = re.exec("ababcababcc");
    shouldBe(match === null ? "null" : match.index, 9);
    shouldBe(re.lastIndex, 10);
}
{
    let re = /(?<=(ab){2})c/y;
    re.lastIndex = 4;
    let match = re.exec("ababcababcc");
    shouldBe(match === null ? "null" : match.index, 4);
    shouldBe(re.lastIndex, 5);
}
{
    let re = /(?<=(ab){2})c/y;
    re.lastIndex = 9;
    let match = re.exec("ababcababcc");
    shouldBe(match === null ? "null" : match.index, 9);
    shouldBe(re.lastIndex, 10);
}
{
    let re = /(?<=(ab){2})c/y;
    re.lastIndex = 10;
    let match = re.exec("ababcababcc");
    shouldBe(match === null ? "null" : match.index, "null");
    shouldBe(re.lastIndex, 0);
}
{
    let re = /(?<=(a|ab){1,2}?c)d/g;
    re.lastIndex = 0;
    let match = re.exec("abacdaabcd");
    shouldBe(match === null ? "null" : match.index, 4);
    shouldBe(re.lastIndex, 5);
}
{
    let re = /(?<=(a|ab){1,2}?c)d/g;
    re.lastIndex = 4;
    let match = re.exec("abacdaabcd");
    shouldBe(match === null ? "null" : match.index, 4);
    shouldBe(re.lastIndex, 5);
}
{
    let re = /(?<=(a|ab){1,2}?c)d/g;
    re.lastIndex = 5;
    let match = re.exec("abacdaabcd");
    shouldBe(match === null ? "null" : match.index, 9);
    shouldBe(re.lastIndex, 10);
}
{
    let re = /(?<=(a|ab){1,2}?c)d/y;
    re.lastIndex = 4;
    let match = re.exec("abacdaabcd");
    shouldBe(match === null ? "null" : match.index, 4);
    shouldBe(re.lastIndex, 5);
}
{
    let re = /(?<=(a|ab){1,2}?c)d/y;
    re.lastIndex = 9;
    let match = re.exec("abacdaabcd");
    shouldBe(match === null ? "null" : match.index, 9);
    shouldBe(re.lastIndex, 10);
}
{
    let re = /(?<=^(?:a|b){1,3}(?:c|d){1,3})e/gm;
    re.lastIndex = 0;
    let match = re.exec("abcde\nabbcdde\nabbbcde");
    shouldBe(match === null ? "null" : match.index, 4);
    shouldBe(re.lastIndex, 5);
}
{
    let re = /(?<=^(?:a|b){1,3}(?:c|d){1,3})e/gm;
    re.lastIndex = 6;
    let match = re.exec("abcde\nabbcdde\nabbbcde");
    shouldBe(match === null ? "null" : match.index, 12);
    shouldBe(re.lastIndex, 13);
}
{
    let re = /(?<=^(?:a|b){1,3}(?:c|d){1,3})e/gm;
    re.lastIndex = 14;
    let match = re.exec("abcde\nabbcdde\nabbbcde");
    shouldBe(match === null ? "null" : match.index, "null");
    shouldBe(re.lastIndex, 0);
}
{
    let re = /(?<=(\d{1,3},)*\d+)x/g;
    re.lastIndex = 0;
    let match = re.exec("1,22,333x 4x x");
    shouldBe(match === null ? "null" : match.index, 8);
    shouldBe(re.lastIndex, 9);
}
{
    let re = /(?<=(\d{1,3},)*\d+)x/g;
    re.lastIndex = 9;
    let match = re.exec("1,22,333x 4x x");
    shouldBe(match === null ? "null" : match.index, 11);
    shouldBe(re.lastIndex, 12);
}
{
    let re = /(?<=(\d{1,3},)*\d+)x/g;
    re.lastIndex = 10;
    let match = re.exec("1,22,333x 4x x");
    shouldBe(match === null ? "null" : match.index, 11);
    shouldBe(re.lastIndex, 12);
}
{
    let re = /(?<=(?:(?:ab)+c)+)d/g;
    re.lastIndex = 0;
    let match = re.exec("ababcabcd d cd");
    shouldBe(match === null ? "null" : match.index, 8);
    shouldBe(re.lastIndex, 9);
}
{
    let re = /(?<=(?:(?:ab)+c)+)d/g;
    re.lastIndex = 8;
    let match = re.exec("ababcabcd d cd");
    shouldBe(match === null ? "null" : match.index, 8);
    shouldBe(re.lastIndex, 9);
}
{
    let re = /(?<=(?:(?:ab)+c)+)d/g;
    re.lastIndex = 9;
    let match = re.exec("ababcabcd d cd");
    shouldBe(match === null ? "null" : match.index, "null");
    shouldBe(re.lastIndex, 0);
}
{
    let re = /(?<=(?:(?:ab)+c)+)d/g;
    re.lastIndex = 10;
    let match = re.exec("ababcabcd d cd");
    shouldBe(match === null ? "null" : match.index, "null");
    shouldBe(re.lastIndex, 0);
}

shouldBe("ababcabcc".replace(/(?<=(?:ab)+)c/g, "-"), "abab-ab-c");
shouldBe("ababcabcc".replace(/(?<=(ab)+)c/g, "[$1]"), "abab[ab]ab[ab]c");
shouldBe("abcbcc".replace(/(?<=(a|b)+)c/g, "[$1]"), "ab[a]b[b]c");
shouldBe("aabcababcc".replace(/(?<=(a|ab){2})c/g, "[$1]"), "aab[a]abab[ab]c");
shouldBe("aabcababcc".replace(/(?<=(a|ab){1,2}?)c/g, "[$1]"), "aab[ab]abab[ab]c");
shouldBe("1,22,333x 4x x".replace(/(?<=(\d{1,3},)*\d+)x/g, "[$1]"), "1,22,333[1,] 4[] x");
shouldBe("aabbc".replace(/(?<=^(a){1,2}(b){1,2})c/g, "[$1$2]"), "aabb[ab]");

shouldBe(stringifyIndices(/(?<=(ab)+)c/d.exec("ababc")), "[[4,5],[0,2]]");
shouldBe(stringifyIndices(/(?<=(a|b)+)c/d.exec("abbac")), "[[4,5],[0,1]]");
shouldBe(stringifyIndices(/(?<=(a|ab){2})c/d.exec("aabc")), "[[3,4],[0,1]]");
shouldBe(stringifyIndices(/(?<=(a|ab){1,2}?c)d/d.exec("abacd")), "[[4,5],[2,3]]");
shouldBe(stringifyIndices(/(?<=(\d{1,3},)*\d+)x/d.exec("1,22,333x")), "[[8,9],[0,2]]");
shouldBe(stringifyIndices(/(?<=^(a){1,2}(b){1,2})c/d.exec("aabbc")), "[[4,5],[0,1],[2,3]]");
shouldBe(stringifyIndices(/(?<=^((ab)+c)+)d/d.exec("ababcabcd")), "[[8,9],[0,5],[0,2]]");
shouldBe(stringifyIndices(/(?<=(a*){2})b/d.exec("aab")), "[[2,3],[0,0]]");
shouldBe(stringifyIndices(/(?<=(a|){2})b/d.exec("ab")), "[[1,2],[0,0]]");
shouldBe(stringifyIndices(/(?<=^(a|){2})b/d.exec("aab")), "[[2,3],[0,1]]");
shouldBe(stringifyIndices(/(?<=y(yb|b){1,2}?c)d/d.exec("xybcd")), "[[4,5],[2,3]]");
shouldBe(stringifyIndices(/(?<=y(yb|b){1,2}c)d/d.exec("xybcd")), "[[4,5],[2,3]]");
