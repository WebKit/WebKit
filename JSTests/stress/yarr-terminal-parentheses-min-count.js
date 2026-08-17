// Greedy non-capturing parentheses in tail position are marked `isTerminal` by
// YarrPattern::checkForTerminalParentheses, which lets both engines skip the
// per-iteration ParenContext that quantified subpatterns normally need. The
// eligibility bound is quantityMinCount <= 1, so `*` and `+` both take that
// path while `{2,}` must not: a minimum of two or more has to be able to retry
// an earlier iteration with a different alternative, which requires exactly the
// inter-iteration state the terminal path drops.
//
// The `+` case additionally has to enforce the RepeatMatcher rule that one
// empty iteration is acceptable while the minimum is unmet but a second is not,
// so the empty-capable bodies below matter. In the Yarr JIT those bodies are
// punted back to the interpreter, which is why this file is run in both
// configurations: only the --useRegExpJIT=0 run reaches the interpreter's
// empty-iteration logic.
//
// Expected values were produced by V8 and cross-checked against both JSC tiers.
// Silent on success.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: got ${actual}, expected ${expected}`);
}

function match(re, str) {
    let m = re.exec(str);
    if (m === null)
        return "null";
    return "[" + Array.from(m).map(x => x === undefined ? "undefined" : JSON.stringify(x)).join(",") + "]@" + m.index;
}

function test(source, flags, str, expected) {
    // Compile once and execute repeatedly so the regexp warms up and the
    // DFG/FTL inlined paths run the same pattern the LLInt did.
    let re = new RegExp(source, flags);
    for (let i = 0; i < testLoopCount; ++i) {
        re.lastIndex = 0;
        shouldBe(match(re, str), expected);
    }
}

// core `+` terminal groups: alternative selection
test("(?:AA|A)+", "", "", "null");
test("(?:AA|A)+", "", "A", "[\"A\"]@0");
test("(?:AA|A)+", "", "AA", "[\"AA\"]@0");
test("(?:AA|A)+", "", "AAA", "[\"AAA\"]@0");
test("(?:AA|A)+", "", "AAAA", "[\"AAAA\"]@0");
test("(?:AA|A)+", "", "B", "null");
test("(?:AA|A)+", "", "AB", "[\"A\"]@0");
test("(?:AA|A)+", "", "AAB", "[\"AA\"]@0");
test("(?:AA|A)+", "", "BAA", "[\"AA\"]@1");
test("(?:AA|A)+", "", "BAAAB", "[\"AAA\"]@1");
test("(?:A|AA)+", "", "", "null");
test("(?:A|AA)+", "", "A", "[\"A\"]@0");
test("(?:A|AA)+", "", "AA", "[\"AA\"]@0");
test("(?:A|AA)+", "", "AAA", "[\"AAA\"]@0");
test("(?:A|AA)+", "", "AAAA", "[\"AAAA\"]@0");
test("(?:A|AA)+", "", "AB", "[\"A\"]@0");
test("(?:A|AA)+", "", "AAB", "[\"AA\"]@0");
test("(?:ab|a)+", "", "", "null");
test("(?:ab|a)+", "", "a", "[\"a\"]@0");
test("(?:ab|a)+", "", "ab", "[\"ab\"]@0");
test("(?:ab|a)+", "", "aba", "[\"aba\"]@0");
test("(?:ab|a)+", "", "abab", "[\"abab\"]@0");
test("(?:ab|a)+", "", "aab", "[\"aab\"]@0");
test("(?:ab|a)+", "", "b", "null");
test("(?:ab|a)+", "", "xabab", "[\"abab\"]@1");
test("(?:a|ab)+", "", "a", "[\"a\"]@0");
test("(?:a|ab)+", "", "ab", "[\"a\"]@0");
test("(?:a|ab)+", "", "abab", "[\"a\"]@0");
test("(?:a|ab)+", "", "aab", "[\"aa\"]@0");
test("(?:abc|ab|a)+", "", "abcaba", "[\"abcaba\"]@0");
test("(?:abc|ab|a)+", "", "abca", "[\"abca\"]@0");
test("(?:abc|ab|a)+", "", "aab", "[\"aab\"]@0");
test("(?:abc|ab|a)+", "", "abc", "[\"abc\"]@0");

// {1,} spelled long-hand must behave identically to `+`
test("(?:AA|A){1,}", "", "A", "[\"A\"]@0");
test("(?:AA|A){1,}", "", "AA", "[\"AA\"]@0");
test("(?:AA|A){1,}", "", "AAA", "[\"AAA\"]@0");
test("(?:AA|A){1,}", "", "B", "null");

// {2,} must stay off the terminal path and stay correct
test("(?:AA|A){2,}", "", "A", "null");
test("(?:AA|A){2,}", "", "AA", "[\"AA\"]@0");
test("(?:AA|A){2,}", "", "AAA", "[\"AAA\"]@0");
test("(?:AA|A){2,}", "", "AAAA", "[\"AAAA\"]@0");
test("(?:AA|A){2,}", "", "AAAAA", "[\"AAAAA\"]@0");
test("(?:AA|A){2,}", "", "AB", "null");
test("(?:AAA|A){2,}", "", "AAA", "[\"AAA\"]@0");
test("(?:AAA|A){2,}", "", "AAAA", "[\"AAAA\"]@0");
test("(?:AAA|A){2,}", "", "AAAAAA", "[\"AAAAAA\"]@0");
test("(?:AAA|A){2,}", "", "AA", "[\"AA\"]@0");
test("(?:AA|A){3,}", "", "AA", "null");
test("(?:AA|A){3,}", "", "AAA", "[\"AAA\"]@0");
test("(?:AA|A){3,}", "", "AAAA", "[\"AAAA\"]@0");
test("(?:AA|A){3,}", "", "AAAAAA", "[\"AAAAAA\"]@0");

// `*` terminal groups: regression coverage for the loop-back arithmetic
test("(?:AA|A)*", "", "", "[\"\"]@0");
test("(?:AA|A)*", "", "A", "[\"A\"]@0");
test("(?:AA|A)*", "", "AA", "[\"AA\"]@0");
test("(?:AA|A)*", "", "AAA", "[\"AAA\"]@0");
test("(?:AA|A)*", "", "B", "[\"\"]@0");
test("(?:AA|A)*", "", "AAB", "[\"AA\"]@0");
test("(?:ab|a)*", "", "", "[\"\"]@0");
test("(?:ab|a)*", "", "a", "[\"a\"]@0");
test("(?:ab|a)*", "", "abab", "[\"abab\"]@0");
test("(?:ab|a)*", "", "b", "[\"\"]@0");
test("(?:ab|a)*", "", "xab", "[\"\"]@0");
test("x(?:ab|a)*", "", "x", "[\"x\"]@0");
test("x(?:ab|a)*", "", "xa", "[\"xa\"]@0");
test("x(?:ab|a)*", "", "xab", "[\"xab\"]@0");
test("x(?:ab|a)*", "", "xabab", "[\"xabab\"]@0");
test("x(?:ab|a)*", "", "y", "null");

// first iteration fails, must backtrack out into a backtrackable prefix
test("a+(?:ab)+", "", "aaab", "[\"aaab\"]@0");
test("a+(?:ab)+", "", "aab", "[\"aab\"]@0");
test("a+(?:ab)+", "", "ab", "null");
test("a+(?:ab)+", "", "aaa", "null");
test("a+(?:ab)+", "", "aaabab", "[\"aaabab\"]@0");
test("(?:a|aa)(?:bc|bd)+", "", "aabc", "[\"aabc\"]@0");
test("(?:a|aa)(?:bc|bd)+", "", "abc", "[\"abc\"]@0");
test("(?:a|aa)(?:bc|bd)+", "", "abd", "[\"abd\"]@0");
test("(?:a|aa)(?:bc|bd)+", "", "aabdbc", "[\"aabdbc\"]@0");
test("a*(?:ab)+", "", "aaab", "[\"aaab\"]@0");
test("a*(?:ab)+", "", "ab", "[\"ab\"]@0");
test("a*(?:ab)+", "", "aaa", "null");
test("[a-z]*(?:0)+", "", "abc", "null");
test("[a-z]*(?:0)+", "", "abc0", "[\"abc0\"]@0");
test("[a-z]*(?:0)+", "", "abc00", "[\"abc00\"]@0");
test("(?:a|aa)(?:ab)+", "", "aaab", "[\"aaab\"]@0");
test("(?:a|aa)(?:ab)+", "", "aab", "[\"aab\"]@0");
test(".*(?:ab)+", "", "xxab", "[\"xxab\"]@0");
test(".*(?:ab)+", "", "xxabab", "[\"xxabab\"]@0");
test(".*(?:ab)+", "", "xx", "null");

// terminal group cannot match at all
test("x(?:ab)+", "", "x", "null");
test("x(?:ab)+", "", "xa", "null");
test("x(?:ab)+", "", "xab", "[\"xab\"]@0");
test("a(?:bc)+", "", "axbc", "null");
test("a(?:bc)+", "", "abc", "[\"abc\"]@0");
test("a(?:bc)+", "", "abcbc", "[\"abcbc\"]@0");
test("a(?:bc)+", "", "a", "null");

// match start scanning: .index must be right
test("(?:a)+", "", "ba", "[\"a\"]@1");
test("(?:a)+", "", "bba", "[\"a\"]@2");
test("(?:a)+", "", "b", "null");
test("(?:a)+", "", "baa", "[\"aa\"]@1");
test("(?:ab|a)+", "", "zzab", "[\"ab\"]@2");
test("(?:ab|a)+", "", "zzzaab", "[\"aab\"]@3");

// empty-capable bodies: one empty iteration is legal for `+`
test("(?:a?)+", "", "", "[\"\"]@0");
test("(?:a?)+", "", "a", "[\"a\"]@0");
test("(?:a?)+", "", "aa", "[\"aa\"]@0");
test("(?:a?)+", "", "b", "[\"\"]@0");
test("(?:|a)+", "", "", "[\"\"]@0");
test("(?:|a)+", "", "a", "[\"a\"]@0");
test("(?:|a)+", "", "aa", "[\"aa\"]@0");
test("(?:|a)+", "", "aaa", "[\"aaa\"]@0");
test("(?:|a)+", "", "b", "[\"\"]@0");
test("(?:a|)+", "", "", "[\"\"]@0");
test("(?:a|)+", "", "a", "[\"a\"]@0");
test("(?:a|)+", "", "aa", "[\"aa\"]@0");
test("(?:a|)+", "", "b", "[\"\"]@0");
test("(?:a*)+", "", "", "[\"\"]@0");
test("(?:a*)+", "", "a", "[\"a\"]@0");
test("(?:a*)+", "", "aa", "[\"aa\"]@0");
test("(?:a*)+", "", "b", "[\"\"]@0");
test("(?:)+", "", "", "[\"\"]@0");
test("(?:)+", "", "a", "[\"\"]@0");
test("x(?:a?)+", "", "x", "[\"x\"]@0");
test("x(?:a?)+", "", "xa", "[\"xa\"]@0");
test("x(?:a?)+", "", "xaa", "[\"xaa\"]@0");
test("x(?:a?)+", "", "y", "null");
test("(?:a?)*", "", "", "[\"\"]@0");
test("(?:a?)*", "", "a", "[\"a\"]@0");
test("(?:a?)*", "", "b", "[\"\"]@0");
test("(?:\\b|a)+", "", "a", "[\"a\"]@0");
test("(?:\\b|a)+", "", "b", "[\"\"]@0");
test("(?:\\b|a)+", "", "", "null");
test("(?:$|a)+", "", "a", "[\"a\"]@0");
test("(?:$|a)+", "", "", "[\"\"]@0");
test("(?:$|a)+", "", "b", "[\"\"]@1");

// anchors and multiple top-level alternatives
test("^a(?:b)+", "", "ab", "[\"ab\"]@0");
test("^a(?:b)+", "", "abb", "[\"abb\"]@0");
test("^a(?:b)+", "", "a", "null");
test("^a(?:b)+", "", "xab", "null");
test("^a(?:b)+|c(?:d)+", "", "ab", "[\"ab\"]@0");
test("^a(?:b)+|c(?:d)+", "", "cd", "[\"cd\"]@0");
test("^a(?:b)+|c(?:d)+", "", "xcdd", "[\"cdd\"]@1");
test("^a(?:b)+|c(?:d)+", "", "a", "null");
test("^a(?:b)+|c(?:d)+", "", "c", "null");
test("(?:a)+$", "", "a", "[\"a\"]@0");
test("(?:a)+$", "", "aa", "[\"aa\"]@0");
test("(?:a)+$", "", "ab", "null");
test("(?:a)+$", "", "ba", "[\"a\"]@1");
test("(?:ab|a)+$", "", "ab", "[\"ab\"]@0");
test("(?:ab|a)+$", "", "aba", "[\"aba\"]@0");
test("(?:ab|a)+$", "", "abx", "null");

// nesting: a generic quantified group inside the terminal group
test("(?:(?:ab)+c|a)+", "", "abc", "[\"abc\"]@0");
test("(?:(?:ab)+c|a)+", "", "ababca", "[\"ababca\"]@0");
test("(?:(?:ab)+c|a)+", "", "a", "[\"a\"]@0");
test("(?:(?:ab)+c|a)+", "", "aabc", "[\"aabc\"]@0");
test("(?:a(?:b|c)d|a)+", "", "abd", "[\"abd\"]@0");
test("(?:a(?:b|c)d|a)+", "", "abdacd", "[\"abdacd\"]@0");
test("(?:a(?:b|c)d|a)+", "", "a", "[\"a\"]@0");
test("(?:a(?:b|c)d|a)+", "", "aabd", "[\"aabd\"]@0");
test("(?:[0-9]+|x)+", "", "12x34", "[\"12x34\"]@0");
test("(?:[0-9]+|x)+", "", "x", "[\"x\"]@0");
test("(?:[0-9]+|x)+", "", "12", "[\"12\"]@0");
test("(?:[0-9]+|x)+", "", "y", "null");

// flags on the core shapes
test("(?:AA|A)+", "i", "aA", "[\"aA\"]@0");
test("(?:AA|A)+", "i", "aa", "[\"aa\"]@0");
test("(?:AA|A)+", "i", "AaA", "[\"AaA\"]@0");
test("(?:AA|A)+", "i", "b", "null");
test("(?:ab|a)+", "i", "AbAb", "[\"AbAb\"]@0");
test("(?:ab|a)+", "i", "aB", "[\"aB\"]@0");
test("(?:AA|A)+", "u", "AA", "[\"AA\"]@0");
test("(?:AA|A)+", "u", "A", "[\"A\"]@0");
test("(?:AA|A)+", "v", "AA", "[\"AA\"]@0");
test("(?:AA|A)+", "v", "A", "[\"A\"]@0");
test("(?:ab|a)+", "y", "abab", "[\"abab\"]@0");
test("(?:ab|a)+", "y", "xab", "null");
test("(?:ab|a)+", "s", "abab", "[\"abab\"]@0");

// long inputs, many iterations
test("(?:AA|A)+", "", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "[\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"]@0");
test("(?:AA|A)+", "", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB", "[\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"]@0");
test("(?:ab|a)+", "", "abababababababababababababababababababababababababababababababababababababababababababababababababab", "[\"abababababababababababababababababababababababababababababababababababababababababababababababababab\"]@0");
test("(?:ab|a)+", "", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "[\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"]@0");
test("x(?:ab|a)*", "", "xabababababababababababababababababababababababababababababababababababababababababababababababababab", "[\"xabababababababababababababababababababababababababababababababababababababababababababababababababab\"]@0");
test("a+(?:ab)+", "", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", "[\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab\"]@0");

// 16-bit subjects. The Yarr JIT compiles 8-bit and 16-bit code separately, so the
// same patterns must be re-run against a subject containing a non-Latin1 character.
test("(?:AA|A)+", "", "\u3042", "null");
test("(?:AA|A)+", "", "A\u3042", "[\"A\"]@0");
test("(?:AA|A)+", "", "AA\u3042", "[\"AA\"]@0");
test("(?:AA|A)+", "", "AAA\u3042", "[\"AAA\"]@0");
test("(?:AA|A)+", "", "AAAA\u3042", "[\"AAAA\"]@0");
test("(?:AA|A)+", "", "B\u3042", "null");
test("(?:AA|A)+", "", "AB\u3042", "[\"A\"]@0");
test("(?:AA|A)+", "", "AAB\u3042", "[\"AA\"]@0");
test("(?:AA|A)+", "", "BAA\u3042", "[\"AA\"]@1");
test("(?:AA|A)+", "", "BAAAB\u3042", "[\"AAA\"]@1");
test("(?:A|AA)+", "", "\u3042", "null");
test("(?:A|AA)+", "", "A\u3042", "[\"A\"]@0");
test("(?:A|AA)+", "", "AA\u3042", "[\"AA\"]@0");
test("(?:A|AA)+", "", "AAA\u3042", "[\"AAA\"]@0");
test("(?:A|AA)+", "", "AAAA\u3042", "[\"AAAA\"]@0");
test("(?:A|AA)+", "", "AB\u3042", "[\"A\"]@0");
test("(?:A|AA)+", "", "AAB\u3042", "[\"AA\"]@0");
test("(?:ab|a)+", "", "\u3042", "null");
test("(?:ab|a)+", "", "a\u3042", "[\"a\"]@0");
test("(?:ab|a)+", "", "ab\u3042", "[\"ab\"]@0");
test("(?:ab|a)+", "", "aba\u3042", "[\"aba\"]@0");
test("(?:ab|a)+", "", "abab\u3042", "[\"abab\"]@0");
test("(?:ab|a)+", "", "aab\u3042", "[\"aab\"]@0");
test("(?:ab|a)+", "", "b\u3042", "null");
test("(?:ab|a)+", "", "xabab\u3042", "[\"abab\"]@1");
test("(?:a|ab)+", "", "a\u3042", "[\"a\"]@0");
test("(?:a|ab)+", "", "ab\u3042", "[\"a\"]@0");
test("(?:a|ab)+", "", "abab\u3042", "[\"a\"]@0");
test("(?:a|ab)+", "", "aab\u3042", "[\"aa\"]@0");
test("(?:abc|ab|a)+", "", "abcaba\u3042", "[\"abcaba\"]@0");
test("(?:abc|ab|a)+", "", "abca\u3042", "[\"abca\"]@0");
test("(?:abc|ab|a)+", "", "aab\u3042", "[\"aab\"]@0");
test("(?:abc|ab|a)+", "", "abc\u3042", "[\"abc\"]@0");
test("(?:AA|A){1,}", "", "A\u3042", "[\"A\"]@0");
test("(?:AA|A){1,}", "", "AA\u3042", "[\"AA\"]@0");
test("(?:AA|A){1,}", "", "AAA\u3042", "[\"AAA\"]@0");
test("(?:AA|A){1,}", "", "B\u3042", "null");
test("(?:AA|A){2,}", "", "A\u3042", "null");
test("(?:AA|A){2,}", "", "AA\u3042", "[\"AA\"]@0");
test("(?:AA|A){2,}", "", "AAA\u3042", "[\"AAA\"]@0");
test("(?:AA|A){2,}", "", "AAAA\u3042", "[\"AAAA\"]@0");
test("(?:AA|A){2,}", "", "AAAAA\u3042", "[\"AAAAA\"]@0");
test("(?:AA|A){2,}", "", "AB\u3042", "null");
test("(?:AAA|A){2,}", "", "AAA\u3042", "[\"AAA\"]@0");
test("(?:AAA|A){2,}", "", "AAAA\u3042", "[\"AAAA\"]@0");
test("(?:AAA|A){2,}", "", "AAAAAA\u3042", "[\"AAAAAA\"]@0");
test("(?:AAA|A){2,}", "", "AA\u3042", "[\"AA\"]@0");
test("(?:AA|A){3,}", "", "AA\u3042", "null");
test("(?:AA|A){3,}", "", "AAA\u3042", "[\"AAA\"]@0");
test("(?:AA|A){3,}", "", "AAAA\u3042", "[\"AAAA\"]@0");
test("(?:AA|A){3,}", "", "AAAAAA\u3042", "[\"AAAAAA\"]@0");
test("(?:AA|A)*", "", "\u3042", "[\"\"]@0");
test("(?:AA|A)*", "", "A\u3042", "[\"A\"]@0");
test("(?:AA|A)*", "", "AA\u3042", "[\"AA\"]@0");
test("(?:AA|A)*", "", "AAA\u3042", "[\"AAA\"]@0");
test("(?:AA|A)*", "", "B\u3042", "[\"\"]@0");
test("(?:AA|A)*", "", "AAB\u3042", "[\"AA\"]@0");
test("(?:ab|a)*", "", "\u3042", "[\"\"]@0");
test("(?:ab|a)*", "", "a\u3042", "[\"a\"]@0");
test("(?:ab|a)*", "", "abab\u3042", "[\"abab\"]@0");
test("(?:ab|a)*", "", "b\u3042", "[\"\"]@0");
test("(?:ab|a)*", "", "xab\u3042", "[\"\"]@0");
test("x(?:ab|a)*", "", "x\u3042", "[\"x\"]@0");
test("x(?:ab|a)*", "", "xa\u3042", "[\"xa\"]@0");
test("x(?:ab|a)*", "", "xab\u3042", "[\"xab\"]@0");
test("x(?:ab|a)*", "", "xabab\u3042", "[\"xabab\"]@0");
test("x(?:ab|a)*", "", "y\u3042", "null");
test("a+(?:ab)+", "", "aaab\u3042", "[\"aaab\"]@0");
test("a+(?:ab)+", "", "aab\u3042", "[\"aab\"]@0");
test("a+(?:ab)+", "", "ab\u3042", "null");
test("a+(?:ab)+", "", "aaa\u3042", "null");
test("a+(?:ab)+", "", "aaabab\u3042", "[\"aaabab\"]@0");
test("(?:a|aa)(?:bc|bd)+", "", "aabc\u3042", "[\"aabc\"]@0");
test("(?:a|aa)(?:bc|bd)+", "", "abc\u3042", "[\"abc\"]@0");
test("(?:a|aa)(?:bc|bd)+", "", "abd\u3042", "[\"abd\"]@0");
test("(?:a|aa)(?:bc|bd)+", "", "aabdbc\u3042", "[\"aabdbc\"]@0");
test("a*(?:ab)+", "", "aaab\u3042", "[\"aaab\"]@0");
test("a*(?:ab)+", "", "ab\u3042", "[\"ab\"]@0");
test("a*(?:ab)+", "", "aaa\u3042", "null");
test("[a-z]*(?:0)+", "", "abc\u3042", "null");
test("[a-z]*(?:0)+", "", "abc0\u3042", "[\"abc0\"]@0");
test("[a-z]*(?:0)+", "", "abc00\u3042", "[\"abc00\"]@0");
test("(?:a|aa)(?:ab)+", "", "aaab\u3042", "[\"aaab\"]@0");
test("(?:a|aa)(?:ab)+", "", "aab\u3042", "[\"aab\"]@0");
test(".*(?:ab)+", "", "xxab\u3042", "[\"xxab\"]@0");
test(".*(?:ab)+", "", "xxabab\u3042", "[\"xxabab\"]@0");
test(".*(?:ab)+", "", "xx\u3042", "null");
test("x(?:ab)+", "", "x\u3042", "null");
test("x(?:ab)+", "", "xa\u3042", "null");
test("x(?:ab)+", "", "xab\u3042", "[\"xab\"]@0");
test("a(?:bc)+", "", "axbc\u3042", "null");
test("a(?:bc)+", "", "abc\u3042", "[\"abc\"]@0");
test("a(?:bc)+", "", "abcbc\u3042", "[\"abcbc\"]@0");
test("a(?:bc)+", "", "a\u3042", "null");
test("(?:a)+", "", "ba\u3042", "[\"a\"]@1");
test("(?:a)+", "", "bba\u3042", "[\"a\"]@2");
test("(?:a)+", "", "b\u3042", "null");
test("(?:a)+", "", "baa\u3042", "[\"aa\"]@1");
test("(?:ab|a)+", "", "zzab\u3042", "[\"ab\"]@2");
test("(?:ab|a)+", "", "zzzaab\u3042", "[\"aab\"]@3");
test("(?:a?)+", "", "\u3042", "[\"\"]@0");
test("(?:a?)+", "", "a\u3042", "[\"a\"]@0");
test("(?:a?)+", "", "aa\u3042", "[\"aa\"]@0");
test("(?:a?)+", "", "b\u3042", "[\"\"]@0");
test("(?:|a)+", "", "\u3042", "[\"\"]@0");
test("(?:|a)+", "", "a\u3042", "[\"a\"]@0");
test("(?:|a)+", "", "aa\u3042", "[\"aa\"]@0");
test("(?:|a)+", "", "aaa\u3042", "[\"aaa\"]@0");
test("(?:|a)+", "", "b\u3042", "[\"\"]@0");
test("(?:a|)+", "", "\u3042", "[\"\"]@0");
test("(?:a|)+", "", "a\u3042", "[\"a\"]@0");
test("(?:a|)+", "", "aa\u3042", "[\"aa\"]@0");
test("(?:a|)+", "", "b\u3042", "[\"\"]@0");
test("(?:a*)+", "", "\u3042", "[\"\"]@0");
test("(?:a*)+", "", "a\u3042", "[\"a\"]@0");
test("(?:a*)+", "", "aa\u3042", "[\"aa\"]@0");
test("(?:a*)+", "", "b\u3042", "[\"\"]@0");
test("(?:)+", "", "\u3042", "[\"\"]@0");
test("(?:)+", "", "a\u3042", "[\"\"]@0");
test("x(?:a?)+", "", "x\u3042", "[\"x\"]@0");
test("x(?:a?)+", "", "xa\u3042", "[\"xa\"]@0");
test("x(?:a?)+", "", "xaa\u3042", "[\"xaa\"]@0");
test("x(?:a?)+", "", "y\u3042", "null");
test("(?:a?)*", "", "\u3042", "[\"\"]@0");
test("(?:a?)*", "", "a\u3042", "[\"a\"]@0");
test("(?:a?)*", "", "b\u3042", "[\"\"]@0");
test("(?:\\b|a)+", "", "a\u3042", "[\"a\"]@0");
test("(?:\\b|a)+", "", "b\u3042", "[\"\"]@0");
test("(?:\\b|a)+", "", "\u3042", "null");
test("(?:$|a)+", "", "a\u3042", "[\"a\"]@0");
test("(?:$|a)+", "", "\u3042", "[\"\"]@1");
test("(?:$|a)+", "", "b\u3042", "[\"\"]@2");
test("^a(?:b)+", "", "ab\u3042", "[\"ab\"]@0");
test("^a(?:b)+", "", "abb\u3042", "[\"abb\"]@0");
test("^a(?:b)+", "", "a\u3042", "null");
test("^a(?:b)+", "", "xab\u3042", "null");
test("^a(?:b)+|c(?:d)+", "", "ab\u3042", "[\"ab\"]@0");
test("^a(?:b)+|c(?:d)+", "", "cd\u3042", "[\"cd\"]@0");
test("^a(?:b)+|c(?:d)+", "", "xcdd\u3042", "[\"cdd\"]@1");
test("^a(?:b)+|c(?:d)+", "", "a\u3042", "null");
test("^a(?:b)+|c(?:d)+", "", "c\u3042", "null");
test("(?:a)+$", "", "a\u3042", "null");
test("(?:a)+$", "", "aa\u3042", "null");
test("(?:a)+$", "", "ab\u3042", "null");
test("(?:a)+$", "", "ba\u3042", "null");
test("(?:ab|a)+$", "", "ab\u3042", "null");
test("(?:ab|a)+$", "", "aba\u3042", "null");
test("(?:ab|a)+$", "", "abx\u3042", "null");
test("(?:(?:ab)+c|a)+", "", "abc\u3042", "[\"abc\"]@0");
test("(?:(?:ab)+c|a)+", "", "ababca\u3042", "[\"ababca\"]@0");
test("(?:(?:ab)+c|a)+", "", "a\u3042", "[\"a\"]@0");
test("(?:(?:ab)+c|a)+", "", "aabc\u3042", "[\"aabc\"]@0");
test("(?:a(?:b|c)d|a)+", "", "abd\u3042", "[\"abd\"]@0");
test("(?:a(?:b|c)d|a)+", "", "abdacd\u3042", "[\"abdacd\"]@0");
test("(?:a(?:b|c)d|a)+", "", "a\u3042", "[\"a\"]@0");
test("(?:a(?:b|c)d|a)+", "", "aabd\u3042", "[\"aabd\"]@0");
test("(?:[0-9]+|x)+", "", "12x34\u3042", "[\"12x34\"]@0");
test("(?:[0-9]+|x)+", "", "x\u3042", "[\"x\"]@0");
test("(?:[0-9]+|x)+", "", "12\u3042", "[\"12\"]@0");
test("(?:[0-9]+|x)+", "", "y\u3042", "null");
test("(?:AA|A)+", "i", "aA\u3042", "[\"aA\"]@0");
test("(?:AA|A)+", "i", "aa\u3042", "[\"aa\"]@0");
test("(?:AA|A)+", "i", "AaA\u3042", "[\"AaA\"]@0");
test("(?:AA|A)+", "i", "b\u3042", "null");
test("(?:ab|a)+", "i", "AbAb\u3042", "[\"AbAb\"]@0");
test("(?:ab|a)+", "i", "aB\u3042", "[\"aB\"]@0");
test("(?:AA|A)+", "u", "AA\u3042", "[\"AA\"]@0");
test("(?:AA|A)+", "u", "A\u3042", "[\"A\"]@0");
test("(?:AA|A)+", "v", "AA\u3042", "[\"AA\"]@0");
test("(?:AA|A)+", "v", "A\u3042", "[\"A\"]@0");
test("(?:ab|a)+", "s", "abab\u3042", "[\"abab\"]@0");
test("(?:AA|A)+", "", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u3042", "[\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"]@0");
test("(?:AA|A)+", "", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB\u3042", "[\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"]@0");
test("(?:ab|a)+", "", "abababababababababababababababababababababababababababababababababababababababababababababababababab\u3042", "[\"abababababababababababababababababababababababababababababababababababababababababababababababababab\"]@0");
test("(?:ab|a)+", "", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\u3042", "[\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"]@0");
test("x(?:ab|a)*", "", "xabababababababababababababababababababababababababababababababababababababababababababababababababab\u3042", "[\"xabababababababababababababababababababababababababababababababababababababababababababababababababab\"]@0");
test("a+(?:ab)+", "", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab\u3042", "[\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab\"]@0");
