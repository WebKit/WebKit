// Structural coverage for terminal-position greedy parentheses: the frame
// layout they share with their own alternative list, the shapes that must stay
// off the terminal path, and the neighbouring Yarr optimizations that run
// either side of checkForTerminalParentheses.
//
// A multi-alternative terminal group puts the group's own backtracking state in
// frame slots 0-1 and its alternative-list state in slot 2, so the multi- and
// many-alternative bodies here are the ones where a frame-sizing mistake would
// have an iteration's bookkeeping overwrite the saved alternative offset. The
// remaining groups pin the eligibility boundary from the outside: a group is
// only in tail position if nothing follows it, so a trailing assertion, a
// lookaround wrapper, or a position inside a lookaround must all keep the
// optimization off, and `{2,}` must keep it off from the quantifier side.
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

// Multi-alternative terminal bodies. checkForTerminalParentheses leaves the terminal group's
// own frame slots at 0-1 and the group's alternative-list backtrack slot at 2, so these are the
// shapes where a frame-layout mistake would have the End term's matchAmount write land on the
// saved alternative offset.
test("(?:AAA|AA|A)+", "", "A", "[\"A\"]@0");
test("(?:AAA|AA|A)+", "", "AA", "[\"AA\"]@0");
test("(?:AAA|AA|A)+", "", "AAA", "[\"AAA\"]@0");
test("(?:AAA|AA|A)+", "", "AAAA", "[\"AAAA\"]@0");
test("(?:AAA|AA|A)+", "", "AAAAA", "[\"AAAAA\"]@0");
test("(?:AAA|AA|A)+", "", "AAAAAA", "[\"AAAAAA\"]@0");
test("(?:AAA|AA|A)+", "", "B", "null");
test("(?:AAA|AA|A)+", "", "AAAAB", "[\"AAAA\"]@0");
test("(?:abc|ab|a|b)+", "", "abcab", "[\"abcab\"]@0");
test("(?:abc|ab|a|b)+", "", "ba", "[\"ba\"]@0");
test("(?:abc|ab|a|b)+", "", "abab", "[\"abab\"]@0");
test("(?:abc|ab|a|b)+", "", "c", "null");
test("(?:abc|ab|a|b)+", "", "aabbc", "[\"aabb\"]@0");
test("(?:aaaa|aaa|aa|a)+", "", "a", "[\"a\"]@0");
test("(?:aaaa|aaa|aa|a)+", "", "aa", "[\"aa\"]@0");
test("(?:aaaa|aaa|aa|a)+", "", "aaa", "[\"aaa\"]@0");
test("(?:aaaa|aaa|aa|a)+", "", "aaaaa", "[\"aaaaa\"]@0");
test("(?:aaaa|aaa|aa|a)+", "", "aaaaaaa", "[\"aaaaaaa\"]@0");
test("(?:AAA|AA|A)*", "", "", "[\"\"]@0");
test("(?:AAA|AA|A)*", "", "A", "[\"A\"]@0");
test("(?:AAA|AA|A)*", "", "AAAA", "[\"AAAA\"]@0");
test("(?:AAA|AA|A)*", "", "B", "[\"\"]@0");
test("x(?:AAA|AA|A)+", "", "xA", "[\"xA\"]@0");
test("x(?:AAA|AA|A)+", "", "xAAAA", "[\"xAAAA\"]@0");
test("x(?:AAA|AA|A)+", "", "x", "null");
test("x(?:AAA|AA|A)+", "", "xB", "null");
test("(?:AAA|AA|A){2,}", "", "A", "null");
test("(?:AAA|AA|A){2,}", "", "AA", "[\"AA\"]@0");
test("(?:AAA|AA|A){2,}", "", "AAA", "[\"AAA\"]@0");
test("(?:AAA|AA|A){2,}", "", "AAAAA", "[\"AAAAA\"]@0");

// A generic quantified subpattern (which does need ParenContext) inside the terminal group, so
// both frame disciplines are live in one pattern.
test("(?:(?:ab)+c|x)+", "", "abc", "[\"abc\"]@0");
test("(?:(?:ab)+c|x)+", "", "abcx", "[\"abcx\"]@0");
test("(?:(?:ab)+c|x)+", "", "ababcx", "[\"ababcx\"]@0");
test("(?:(?:ab)+c|x)+", "", "x", "[\"x\"]@0");
test("(?:(?:ab)+c|x)+", "", "ab", "null");
test("(?:(?:a|bb)+c|x)+", "", "ac", "[\"ac\"]@0");
test("(?:(?:a|bb)+c|x)+", "", "bbc", "[\"bbc\"]@0");
test("(?:(?:a|bb)+c|x)+", "", "abbcx", "[\"abbcx\"]@0");
test("(?:(?:a|bb)+c|x)+", "", "x", "[\"x\"]@0");
test("(?:(?:a|bb)+c|x)+", "", "bb", "null");
test("(?:(?:a{2,3})b|c)+", "", "aab", "[\"aab\"]@0");
test("(?:(?:a{2,3})b|c)+", "", "aaab", "[\"aaab\"]@0");
test("(?:(?:a{2,3})b|c)+", "", "aabc", "[\"aabc\"]@0");
test("(?:(?:a{2,3})b|c)+", "", "c", "[\"c\"]@0");
test("(?:(?:a{2,3})b|c)+", "", "ab", "null");
test("(?:(?:ab|a)+c|x)+", "", "abc", "[\"abc\"]@0");
test("(?:(?:ab|a)+c|x)+", "", "aac", "[\"aac\"]@0");
test("(?:(?:ab|a)+c|x)+", "", "abababc", "[\"abababc\"]@0");
test("(?:(?:ab|a)+c|x)+", "", "x", "[\"x\"]@0");

// A terminal group in each top-level alternative.
test("a(?:b)+|c(?:d)+|e(?:f)+", "", "ab", "[\"ab\"]@0");
test("a(?:b)+|c(?:d)+|e(?:f)+", "", "cd", "[\"cd\"]@0");
test("a(?:b)+|c(?:d)+|e(?:f)+", "", "ef", "[\"ef\"]@0");
test("a(?:b)+|c(?:d)+|e(?:f)+", "", "abb", "[\"abb\"]@0");
test("a(?:b)+|c(?:d)+|e(?:f)+", "", "cddd", "[\"cddd\"]@0");
test("a(?:b)+|c(?:d)+|e(?:f)+", "", "a", "null");
test("a(?:b)+|c(?:d)+|e(?:f)+", "", "c", "null");
test("a(?:b)+|c(?:d)+|e(?:f)+", "", "e", "null");
test("a(?:b)+|c(?:d)+|e(?:f)+", "", "g", "null");
test("(?:x|y)(?:AA|A)+|z(?:B)+", "", "xA", "[\"xA\"]@0");
test("(?:x|y)(?:AA|A)+|z(?:B)+", "", "yAA", "[\"yAA\"]@0");
test("(?:x|y)(?:AA|A)+|z(?:B)+", "", "zB", "[\"zB\"]@0");
test("(?:x|y)(?:AA|A)+|z(?:B)+", "", "zBB", "[\"zBB\"]@0");
test("(?:x|y)(?:AA|A)+|z(?:B)+", "", "x", "null");
test("(?:x|y)(?:AA|A)+|z(?:B)+", "", "z", "null");

// Terminal group inside a lookahead or lookbehind is not a top-level body tail, so it must not
// be marked terminal.
test("(?=(?:ab)+)a", "", "ab", "[\"a\"]@0");
test("(?=(?:ab)+)a", "", "abab", "[\"a\"]@0");
test("(?=(?:ab)+)a", "", "ac", "null");
test("(?:(?=(?:ab)+)ab)+c", "", "ababc", "[\"ababc\"]@0");
test("(?:(?=(?:ab)+)ab)+c", "", "abc", "[\"abc\"]@0");
test("(?:(?=(?:ab)+)ab)+c", "", "ac", "null");
test("(?<=(?:ab)+)c", "", "abc", "[\"c\"]@2");
test("(?<=(?:ab)+)c", "", "ababc", "[\"c\"]@4");
test("(?<=(?:ab)+)c", "", "ac", "null");
test("(?!(?:ab)+)a.", "", "ac", "[\"ac\"]@0");
test("(?!(?:ab)+)a.", "", "ab", "null");

// The terminal group wrapped by an assertion that follows it: the group is no longer in tail
// position, so the optimization must not apply.
test("(?:ab|a)+$", "", "ab", "[\"ab\"]@0");
test("(?:ab|a)+$", "", "aba", "[\"aba\"]@0");
test("(?:ab|a)+$", "", "abx", "null");
test("(?:ab|a)+(?=x)", "", "abx", "[\"ab\"]@0");
test("(?:ab|a)+(?=x)", "", "ax", "[\"a\"]@0");
test("(?:ab|a)+(?=x)", "", "ab", "null");
test("(?:ab|a)+\\b", "", "ab", "[\"ab\"]@0");
test("(?:ab|a)+\\b", "", "ab.", "[\"ab\"]@0");
test("(?:ab|a)+\\b", "", "a", "[\"a\"]@0");
test("(?:ab|a)+(?![a-z])", "", "ab", "[\"ab\"]@0");
test("(?:ab|a)+(?![a-z])", "", "abx", "null");
test("(?:ab|a)+(?![a-z])", "", "ab.", "[\"ab\"]@0");

// Anchored and multiline: recomputeStartsWithBOL and optimizeBOL run either side of
// checkForTerminalParentheses and copy alternatives around.
test("^(?:ab|a)+", "", "abab", "[\"abab\"]@0");
test("^(?:ab|a)+", "", "a", "[\"a\"]@0");
test("^(?:ab|a)+", "", "xab", "null");
test("^(?:ab|a)+", "m", "x\nabab", "[\"abab\"]@2");
test("^(?:ab|a)+", "m", "x\na", "[\"a\"]@2");
test("^(?:ab|a)+", "m", "x", "null");
test("^a(?:b|bc)+", "m", "x\nabc", "[\"ab\"]@2");
test("^a(?:b|bc)+", "m", "x\nab", "[\"ab\"]@2");
test("^a(?:b|bc)+", "m", "x\na", "null");
test("^(?:AA|A)*", "m", "x\nAA", "[\"\"]@0");
test("^(?:AA|A)*", "m", "x\n", "[\"\"]@0");
test("(?:^|x)(?:AA|A)+", "", "AA", "[\"AA\"]@0");
test("(?:^|x)(?:AA|A)+", "", "xA", "[\"xA\"]@0");
test("(?:^|x)(?:AA|A)+", "", "yA", "null");

// Interaction with the string-list optimization, which lives in the same function and is the
// block the eligibility restructuring re-scoped.
test("^(?:foo|bar|baz)", "", "foo", "[\"foo\"]@0");
test("^(?:foo|bar|baz)", "", "bar", "[\"bar\"]@0");
test("^(?:foo|bar|baz)", "", "baz", "[\"baz\"]@0");
test("^(?:foo|bar|baz)", "", "qux", "null");
test("^(?:foo|bar)(?:x)+", "", "foox", "[\"foox\"]@0");
test("^(?:foo|bar)(?:x)+", "", "barxx", "[\"barxx\"]@0");
test("^(?:foo|bar)(?:x)+", "", "foo", "null");
test("^(?:foo|bar)(?:x)+", "", "bazx", "null");
test("^(?:foo|foobar)+", "", "foobar", "[\"foo\"]@0");
test("^(?:foo|foobar)+", "", "foofoo", "[\"foofoo\"]@0");
test("^(?:foo|foobar)+", "", "foo", "[\"foo\"]@0");
test("^(?:foo|foobar)+", "", "bar", "null");

// Interaction with dot-star wrapping and auto-possessification.
test(".*(?:ab|a)+", "", "xxab", "[\"xxab\"]@0");
test(".*(?:ab|a)+", "", "xxa", "[\"xxa\"]@0");
test(".*(?:ab|a)+", "", "xx", "null");
test("(?:.*)(?:ab)+", "", "xxab", "[\"xxab\"]@0");
test("(?:.*)(?:ab)+", "", "xx", "null");
test("a+(?:b|bc)+", "", "aab", "[\"aab\"]@0");
test("a+(?:b|bc)+", "", "aabc", "[\"aab\"]@0");
test("a+(?:b|bc)+", "", "aa", "null");
test("[0-9]+(?:ab)+", "", "12ab", "[\"12ab\"]@0");
test("[0-9]+(?:ab)+", "", "12abab", "[\"12abab\"]@0");
test("[0-9]+(?:ab)+", "", "12a", "null");
test("\\d+(?:x)+y", "", "12xy", "[\"12xy\"]@0");
test("\\d+(?:x)+y", "", "12xxy", "[\"12xxy\"]@0");
test("\\d+(?:x)+y", "", "12y", "null");

// Boyer-Moore / fixed-prefix search paths in front of a terminal group.
test("hello(?:world|w)+", "", "helloworld", "[\"helloworld\"]@0");
test("hello(?:world|w)+", "", "hellow", "[\"hellow\"]@0");
test("hello(?:world|w)+", "", "xhelloworldw", "[\"helloworldw\"]@1");
test("hello(?:world|w)+", "", "hello", "null");
test("prefix(?:ab|a)+", "", "prefixabab", "[\"prefixabab\"]@0");
test("prefix(?:ab|a)+", "", "prefixa", "[\"prefixa\"]@0");
test("prefix(?:ab|a)+", "", "prefix", "null");
test("prefix(?:ab|a)+", "", "zzprefixab", "[\"prefixab\"]@2");

// Character classes and unicode sets inside the terminal group.
test("(?:[a-c]{2}|[a-c])+", "", "ab", "[\"ab\"]@0");
test("(?:[a-c]{2}|[a-c])+", "", "abc", "[\"abc\"]@0");
test("(?:[a-c]{2}|[a-c])+", "", "abcabc", "[\"abcabc\"]@0");
test("(?:[a-c]{2}|[a-c])+", "", "d", "null");
test("(?:[a-c]{2}|[a-c])+", "", "abd", "[\"ab\"]@0");
test("(?:[^x]{2}|[^x])+", "", "ab", "[\"ab\"]@0");
test("(?:[^x]{2}|[^x])+", "", "abc", "[\"abc\"]@0");
test("(?:[^x]{2}|[^x])+", "", "x", "null");
test("(?:[^x]{2}|[^x])+", "", "abx", "[\"ab\"]@0");
test("(?:\\w\\w|\\w)+", "", "ab", "[\"ab\"]@0");
test("(?:\\w\\w|\\w)+", "", "abc", "[\"abc\"]@0");
test("(?:\\w\\w|\\w)+", "", "!", "null");
test("(?:\\w\\w|\\w)+", "", "ab!", "[\"ab\"]@0");
test("(?:[\\q{ab}]|a)+", "v", "ab", "[\"ab\"]@0");
test("(?:[\\q{ab}]|a)+", "v", "aab", "[\"aab\"]@0");
test("(?:[\\q{ab}]|a)+", "v", "a", "[\"a\"]@0");
test("(?:[\\q{ab}]|a)+", "v", "b", "null");
test("(?:[\\q{}]|a)+", "v", "a", "[\"a\"]@0");
test("(?:[\\q{}]|a)+", "v", "", "[\"\"]@0");
test("(?:[\\q{}]|a)+", "v", "b", "[\"\"]@0");
test("(?:\\p{ASCII}{2}|\\p{ASCII})+", "u", "ab", "[\"ab\"]@0");
test("(?:\\p{ASCII}{2}|\\p{ASCII})+", "u", "abc", "[\"abc\"]@0");

// Non-BMP content, where the JIT's 16-bit path and surrogate handling apply.
test("(?:\\u{1F600}\\u{1F600}|\\u{1F600})+", "u", "\ud83d\ude00", "[\"\ud83d\ude00\"]@0");
test("(?:\\u{1F600}\\u{1F600}|\\u{1F600})+", "u", "\ud83d\ude00\ud83d\ude00", "[\"\ud83d\ude00\ud83d\ude00\"]@0");
test("(?:\\u{1F600}\\u{1F600}|\\u{1F600})+", "u", "\ud83d\ude00\ud83d\ude00\ud83d\ude00", "[\"\ud83d\ude00\ud83d\ude00\ud83d\ude00\"]@0");
test("(?:\\u{1F600}\\u{1F600}|\\u{1F600})+", "u", "x", "null");
test("(?:ab|a)+", "u", "abab", "[\"abab\"]@0");
test("(?:ab|a)+", "u", "a\ud83d\ude00", "[\"a\"]@0");
test("x(?:ab|a)+", "u", "xab\ud83d\ude00", "[\"xab\"]@0");
test("x(?:ab|a)+", "u", "x\ud83d\ude00", "null");

// Case-insensitive matching, which changes character-class lowering.
test("(?:AA|A)+", "i", "aa", "[\"aa\"]@0");
test("(?:AA|A)+", "i", "Aa", "[\"Aa\"]@0");
test("(?:AA|A)+", "i", "aAa", "[\"aAa\"]@0");
test("(?:AA|A)+", "i", "b", "null");
test("(?:SS|S)+", "i", "ss", "[\"ss\"]@0");
test("(?:SS|S)+", "i", "\u00df", "null");
test("(?:SS|S)+", "i", "Ss", "[\"Ss\"]@0");
test("(?:\\u0130|i)+", "iu", "i", "[\"i\"]@0");
test("(?:\\u0130|i)+", "iu", "\u0130", "[\"\u0130\"]@0");
test("(?:\\u0130|i)+", "iu", "I", "[\"I\"]@0");

// Sticky and global with a terminal group: lastIndex progression must be right.
test("(?:AA|A)+", "g", "AABAAA", "[\"AA\"]@0");
test("(?:AA|A)+", "g", "B", "null");
test("(?:AA|A)+", "y", "AAB", "[\"AA\"]@0");
test("(?:AA|A)+", "y", "BAA", "null");
test("(?:ab|a)+", "gy", "ababab", "[\"ababab\"]@0");

// Long subjects: many iterations, and shapes that would blow up if an iteration ever failed to
// make progress.
test("(?:AAA|AA|A)+", "", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "[\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"]@0");
test("(?:AAA|AA|A)+", "", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB", "[\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"]@0");
test("(?:ab|a)+", "", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "[\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"]@0");
test("(?:ab|a)+", "", "abababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababab", "[\"abababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababab\"]@0");
test("(?:ab|a)*", "", "abababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababab", "[\"abababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababababab\"]@0");
test("x(?:AAA|AA|A)+", "", "xAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "[\"xAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"]@0");
test("a+(?:ab)+", "", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab", "[\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab\"]@0");
test("(?:(?:ab)+c|x)+", "", "ababababababababababababababababababababababababababababababababababababababababababababababababababc", "[\"ababababababababababababababababababababababababababababababababababababababababababababababababababc\"]@0");
test("(?:a?)+", "", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "[\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"]@0");
test("(?:|a)+", "", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "[\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"]@0");
test("(?:AA|A)+", "", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\u3042", "[\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"]@0");
test("(?:AAA|AA|A)+", "", "\u3042AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", "[\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"]@1");

// Many terminal groups in one pattern, to grow the Yarr frame.
test("a(?:b)+|c(?:d)+|e(?:f)+|g(?:h)+|i(?:j)+|k(?:l)+|m(?:n)+|o(?:p)+", "", "op", "[\"op\"]@0");
test("(?:a(?:z)+|b(?:z)+|c(?:z)+|d(?:z)+|e(?:z)+|f(?:z)+|g(?:z)+|h(?:z)+|i(?:z)+|j(?:z)+|k(?:z)+|l(?:z)+)", "", "lz", "[\"lz\"]@0");
