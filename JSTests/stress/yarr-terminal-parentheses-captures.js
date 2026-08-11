// checkForTerminalParentheses used to bail out of the whole function whenever
// the pattern had observable capture groups, so no capturing pattern ever got a
// terminal tail group. It now only skips the string-list optimization in that
// case, which means patterns like /(a)(?:b|bc)+/ and /(a)(?:b|bc)*/ take the
// terminal path and no longer allocate per-iteration ParenContext.
//
// What keeps that sound is the term-level gate: the tail group itself must be
// non-capturing and must contain no captures (PatternTerm::containsAnyCaptures),
// so nothing inside it can change across iterations and there is no capture
// state to restore when an iteration fails. That predicate is now the only
// thing standing between this optimization and a wrong capture array, so the
// cases below pin down both sides of it: captures outside the tail group (which
// must still be reported correctly, and must be unwound when the tail group
// forces a backtrack), and captures inside it or inside a nested lookaround
// (which must keep the group off the terminal path entirely).
//
// `exec` compiles with IncludeSubpatterns while `test` compiles MatchOnly, and
// the two modes disagree about whether captures are observable, so both are
// exercised. Backreferences are included because they force captures to be
// observable in MatchOnly mode too.
//
// Expected values were produced by V8 and cross-checked against both JSC tiers.
// Silent on success.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: got ${actual}, expected ${expected}`);
}

function repeat(re, body) {
    // Compile once and execute repeatedly so the regexp warms up and the
    // DFG/FTL inlined paths run the same pattern the LLInt did.
    for (let i = 0; i < testLoopCount; ++i) {
        re.lastIndex = 0;
        body();
    }
}

function testExec(source, flags, str, expected) {
    let re = new RegExp(source, flags);
    repeat(re, () => {
        let m = re.exec(str);
        let actual = m === null
            ? "null"
            : "[" + Array.from(m).map(x => x === undefined ? "undefined" : JSON.stringify(x)).join(",") + "]@" + m.index;
        shouldBe(actual, expected);
    });
}

function testGroups(source, flags, str, expected) {
    let re = new RegExp(source, flags);
    repeat(re, () => {
        let m = re.exec(str);
        let actual;
        if (m === null)
            actual = "null";
        else {
            let groups = m.groups;
            actual = Object.keys(groups).sort()
                .map(k => k + "=" + (groups[k] === undefined ? "undefined" : JSON.stringify(groups[k])))
                .join(",");
        }
        shouldBe(actual, expected);
    });
}

function testIndices(source, flags, str, expected) {
    let re = new RegExp(source, flags + "d");
    repeat(re, () => {
        let m = re.exec(str);
        let actual = m === null ? "null" : JSON.stringify(m.indices.map(x => x === undefined ? null : x));
        shouldBe(actual, expected);
    });
}

function testTest(source, flags, str, expected) {
    let re = new RegExp(source, flags);
    repeat(re, () => {
        shouldBe(re.test(str), expected);
    });
}

function testReplace(source, flags, str, replacement, expected) {
    let re = new RegExp(source, flags);
    repeat(re, () => {
        shouldBe(str.replace(re, replacement), expected);
    });
}

function testSplit(source, flags, str, expected) {
    let re = new RegExp(source, flags);
    repeat(re, () => {
        shouldBe(JSON.stringify(str.split(re)), expected);
    });
}

function testMatchAll(source, flags, str, expected) {
    let re = new RegExp(source, flags);
    repeat(re, () => {
        let all = Array.from(str.matchAll(re))
            .map(m => Array.from(m).map(x => x === undefined ? null : x).join("|") + "@" + m.index);
        shouldBe(JSON.stringify(all), expected);
    });
}

function testLastIndex(source, flags, str, expected) {
    // A fresh regexp per repetition: this walks lastIndex forward, so it cannot
    // share state between repetitions.
    for (let i = 0; i < testLoopCount; ++i) {
        let re = new RegExp(source, flags);
        let steps = [];
        for (let step = 0; step < 8; ++step) {
            let m = re.exec(str);
            if (m === null) {
                steps.push("null@" + re.lastIndex);
                break;
            }
            steps.push(m[0] + "@" + m.index + ":" + re.lastIndex);
        }
        shouldBe(JSON.stringify(steps), expected);
    }
}

// A capture group before a terminal `+` group: eligible, because the tail group itself holds no
// captures.
testExec("(a)(?:b|bc)+", "", "abcb", "[\"ab\",\"a\"]@0");
testExec("(a)(?:b|bc)+", "", "ab", "[\"ab\",\"a\"]@0");
testExec("(a)(?:b|bc)+", "", "abb", "[\"abb\",\"a\"]@0");
testExec("(a)(?:b|bc)+", "", "abc", "[\"ab\",\"a\"]@0");
testExec("(a)(?:b|bc)+", "", "a", "null");
testExec("(a)(?:b|bc)+", "", "xabb", "[\"abb\",\"a\"]@1");
testExec("(a)(?:bc|b)+", "", "abcb", "[\"abcb\",\"a\"]@0");
testExec("(a)(?:bc|b)+", "", "ab", "[\"ab\",\"a\"]@0");
testExec("(a)(?:bc|b)+", "", "abcbc", "[\"abcbc\",\"a\"]@0");
testExec("(a)(?:bc|b)+", "", "az", "null");
testExec("(\\d)(?:ab|a)+", "", "1ab", "[\"1ab\",\"1\"]@0");
testExec("(\\d)(?:ab|a)+", "", "1aab", "[\"1aab\",\"1\"]@0");
testExec("(\\d)(?:ab|a)+", "", "1a", "[\"1a\",\"1\"]@0");
testExec("(\\d)(?:ab|a)+", "", "1b", "null");
testExec("(a|aa)(?:bc|bd)+", "", "aabc", "[\"aabc\",\"aa\"]@0");
testExec("(a|aa)(?:bc|bd)+", "", "abc", "[\"abc\",\"a\"]@0");
testExec("(a|aa)(?:bc|bd)+", "", "aabdbc", "[\"aabdbc\",\"aa\"]@0");
testExec("(a|aa)(?:bc|bd)+", "", "aa", "null");
testExec("(a+)(?:ab)+", "", "aaab", "[\"aaab\",\"aa\"]@0");
testExec("(a+)(?:ab)+", "", "aab", "[\"aab\",\"a\"]@0");
testExec("(a+)(?:ab)+", "", "aaa", "null");
testExec("(a)(b)(?:c|cd)+", "", "abcd", "[\"abc\",\"a\",\"b\"]@0");
testExec("(a)(b)(?:c|cd)+", "", "abc", "[\"abc\",\"a\",\"b\"]@0");
testExec("(a)(b)(?:c|cd)+", "", "abcc", "[\"abcc\",\"a\",\"b\"]@0");

// The same shapes with `*`, which is eligible on the same grounds.
testExec("(a)(?:b|bc)*", "", "abcb", "[\"ab\",\"a\"]@0");
testExec("(a)(?:b|bc)*", "", "ab", "[\"ab\",\"a\"]@0");
testExec("(a)(?:b|bc)*", "", "a", "[\"a\",\"a\"]@0");
testExec("(a)(?:b|bc)*", "", "b", "null");
testExec("(a)(?:b|bc)*", "", "xa", "[\"a\",\"a\"]@1");
testExec("(a)(?:bc|b)*", "", "abcb", "[\"abcb\",\"a\"]@0");
testExec("(a)(?:bc|b)*", "", "ab", "[\"ab\",\"a\"]@0");
testExec("(a)(?:bc|b)*", "", "a", "[\"a\",\"a\"]@0");
testExec("(\\d)(?:ab|a)*", "", "1ab", "[\"1ab\",\"1\"]@0");
testExec("(\\d)(?:ab|a)*", "", "1", "[\"1\",\"1\"]@0");
testExec("(\\d)(?:ab|a)*", "", "1aab", "[\"1aab\",\"1\"]@0");
testExec("(a+)(?:ab)*", "", "aaab", "[\"aaa\",\"aaa\"]@0");
testExec("(a+)(?:ab)*", "", "aaa", "[\"aaa\",\"aaa\"]@0");
testExec("(a+)(?:ab)*", "", "a", "[\"a\",\"a\"]@0");

// Group that must be left undefined while the pattern still ends in a terminal group.
testExec("(?:(x)|y)a(?:b)+", "", "xab", "[\"xab\",\"x\"]@0");
testExec("(?:(x)|y)a(?:b)+", "", "yab", "[\"yab\",undefined]@0");
testExec("(?:(x)|y)a(?:b)+", "", "xabb", "[\"xabb\",\"x\"]@0");
testExec("(?:(x)|y)a(?:b)+", "", "zab", "null");
testExec("(x)?a(?:b)+", "", "xab", "[\"xab\",\"x\"]@0");
testExec("(x)?a(?:b)+", "", "ab", "[\"ab\",undefined]@0");
testExec("(x)?a(?:b)+", "", "abb", "[\"abb\",undefined]@0");
testExec("(?:a|b)+|(c)", "", "ab", "[\"ab\",undefined]@0");
testExec("(?:a|b)+|(c)", "", "c", "[\"c\",\"c\"]@0");
testExec("(?:a|b)+|(c)", "", "d", "null");
testExec("(?:a|b)+|(c)(?:d)+", "", "ab", "[\"ab\",undefined]@0");
testExec("(?:a|b)+|(c)(?:d)+", "", "cd", "[\"cd\",\"c\"]@0");
testExec("(?:a|b)+|(c)(?:d)+", "", "cdd", "[\"cdd\",\"c\"]@0");
testExec("(?:a|b)+|(c)(?:d)+", "", "c", "null");

// Backreference to an outer group from inside the terminal group. Forces ignoreCaptures = false
// in both execution modes.
testExec("(a)(?:\\1|b)+", "", "aab", "[\"aab\",\"a\"]@0");
testExec("(a)(?:\\1|b)+", "", "ab", "[\"ab\",\"a\"]@0");
testExec("(a)(?:\\1|b)+", "", "aa", "[\"aa\",\"a\"]@0");
testExec("(a)(?:\\1|b)+", "", "a", "null");
testExec("(a)(?:\\1|b)+", "", "aba", "[\"aba\",\"a\"]@0");
testExec("(ab)(?:\\1|c)+", "", "ababc", "[\"ababc\",\"ab\"]@0");
testExec("(ab)(?:\\1|c)+", "", "abc", "[\"abc\",\"ab\"]@0");
testExec("(ab)(?:\\1|c)+", "", "abab", "[\"abab\",\"ab\"]@0");
testExec("(ab)(?:\\1|c)+", "", "ab", "null");
testExec("(a)?(?:\\1|b)+", "", "ab", "[\"ab\",\"a\"]@0");
testExec("(a)?(?:\\1|b)+", "", "b", "[\"b\",undefined]@0");
testExec("(a)?(?:\\1|b)+", "", "aa", "[\"aa\",\"a\"]@0");
testExec("(a)?(?:\\1|b)+", "", "bb", "[\"bb\",undefined]@0");

// Captures inside the group keep it non-terminal; verify they are still right.
testExec("(?:(a)|b)+", "", "ab", "[\"ab\",undefined]@0");
testExec("(?:(a)|b)+", "", "ba", "[\"ba\",\"a\"]@0");
testExec("(?:(a)|b)+", "", "b", "[\"b\",undefined]@0");
testExec("(?:(a)|b)+", "", "aab", "[\"aab\",undefined]@0");
testExec("(?:(a)|(b))+", "", "ab", "[\"ab\",undefined,\"b\"]@0");
testExec("(?:(a)|(b))+", "", "ba", "[\"ba\",\"a\",undefined]@0");
testExec("(?:(a)|(b))+", "", "aa", "[\"aa\",\"a\",undefined]@0");
testExec("x(?:(a)|b)+", "", "xab", "[\"xab\",undefined]@0");
testExec("x(?:(a)|b)+", "", "xba", "[\"xba\",\"a\"]@0");
testExec("x(?:(a)|b)+", "", "xb", "[\"xb\",undefined]@0");

// Captures nested in a lookahead/lookbehind inside the group: also non-terminal.
testExec("(?:(?=(a))a|b)+", "", "ab", "[\"ab\",undefined]@0");
testExec("(?:(?=(a))a|b)+", "", "ba", "[\"ba\",\"a\"]@0");
testExec("(?:(?=(a))a|b)+", "", "b", "[\"b\",undefined]@0");
testExec("(?:(?<=(a))b|c)+", "", "abc", "[\"bc\",undefined]@1");
testExec("(?:(?<=(a))b|c)+", "", "cc", "[\"cc\",undefined]@0");
testExec("(?:(?<=(a))b|c)+", "", "ab", "[\"b\",\"a\"]@1");

// Capture in a lookbehind before the terminal group.
testExec("(?<=(x))(?:ab)+", "", "xab", "[\"ab\",\"x\"]@1");
testExec("(?<=(x))(?:ab)+", "", "xabab", "[\"abab\",\"x\"]@1");
testExec("(?<=(x))(?:ab)+", "", "yab", "null");
testExec("(?<=(x))(?:ab|a)+", "", "xaab", "[\"aab\",\"x\"]@1");
testExec("(?<=(x))(?:ab|a)+", "", "xab", "[\"ab\",\"x\"]@1");

// Named groups and duplicate named groups.
testExec("(?<n>a)(?:b|bc)+", "", "abcb", "[\"ab\",\"a\"]@0");
testExec("(?<n>a)(?:b|bc)+", "", "ab", "[\"ab\",\"a\"]@0");
testExec("(?<n>a)(?:b|bc)+", "", "a", "null");
testGroups("(?<n>a)(?:b|bc)+", "", "abcb", "n=\"a\"");
testGroups("(?<n>a)(?:b|bc)+", "", "ab", "n=\"a\"");
testGroups("(?<n>a)(?:b|bc)+", "", "a", "null");
testGroups("(?<n>a)(?:b|bc)*", "", "ab", "n=\"a\"");
testGroups("(?<n>a)(?:b|bc)*", "", "a", "n=\"a\"");
testExec("(?:(?<n>x)|(?<n>y))(?:AA|A)+", "v", "xAA", "[\"xAA\",\"x\",undefined]@0");
testExec("(?:(?<n>x)|(?<n>y))(?:AA|A)+", "v", "yA", "[\"yA\",undefined,\"y\"]@0");
testExec("(?:(?<n>x)|(?<n>y))(?:AA|A)+", "v", "xB", "null");
testGroups("(?:(?<n>x)|(?<n>y))(?:AA|A)+", "v", "xAA", "n=\"x\"");
testGroups("(?:(?<n>x)|(?<n>y))(?:AA|A)+", "v", "yA", "n=\"y\"");

// hasIndices: capture extents must survive the terminal path.
testIndices("(a)(?:b|bc)+", "", "abcb", "[[0,2],[0,1]]");
testIndices("(a)(?:b|bc)+", "", "ab", "[[0,2],[0,1]]");
testIndices("(a)(?:b|bc)*", "", "ab", "[[0,2],[0,1]]");
testIndices("(a)(?:b|bc)*", "", "a", "[[0,1],[0,1]]");
testIndices("(a|aa)(?:bc|bd)+", "", "aabc", "[[0,4],[0,2]]");
testIndices("(x)?a(?:b)+", "", "xab", "[[0,3],[0,1]]");
testIndices("(x)?a(?:b)+", "", "ab", "[[0,2],null]");

// `test` goes through MatchOnly compilation, `exec` through IncludeSubpatterns. Both modes must
// agree on whether there is a match.
testTest("(a)(?:b|bc)+", "", "abcb", true);
testTest("(a)(?:b|bc)+", "", "ab", true);
testTest("(a)(?:b|bc)+", "", "a", false);
testTest("(a)(?:b|bc)+", "", "b", false);
testTest("(a)(?:b|bc)*", "", "ab", true);
testTest("(a)(?:b|bc)*", "", "a", true);
testTest("(a)(?:b|bc)*", "", "b", false);
testTest("(a)(?:\\1|b)+", "", "aab", true);
testTest("(a)(?:\\1|b)+", "", "ab", true);
testTest("(a)(?:\\1|b)+", "", "a", false);
testTest("(?:(a)|b)+", "", "ab", true);
testTest("(?:(a)|b)+", "", "c", false);
testTest("(?<=(x))(?:ab)+", "", "xab", true);
testTest("(?<=(x))(?:ab)+", "", "yab", false);

// Global replace / split / matchAll over the newly marked shapes.
testReplace("(a)(?:b|bc)+", "g", "abcbabc", "<$1>", "<a>cb<a>c");
testReplace("(a)(?:b|bc)+", "g", "abab", "<$1>", "<a><a>");
testReplace("(a)(?:b|bc)+", "g", "a", "<$1>", "a");
testReplace("(a)(?:b|bc)*", "g", "abcb", "<$1>", "<a>cb");
testReplace("(a)(?:b|bc)*", "g", "aa", "<$1>", "<a><a>");
testReplace("(?:AA|A)+", "g", "AAABAA", "-", "-B-");
testReplace("(?:AA|A)+", "g", "B", "-", "B");
testSplit("(?:,|;)+", "", "a,b;;c", "[\"a\",\"b\",\"c\"]");
testSplit("(?:,|;)+", "", "abc", "[\"abc\"]");
testSplit("(a)(?:b)+", "", "xabby", "[\"x\",\"a\",\"y\"]");
testSplit("(a)(?:b)+", "", "xy", "[\"xy\"]");
testMatchAll("(a)(?:b|bc)+", "g", "abcbabb", "[\"ab|a@0\",\"abb|a@4\"]");
testMatchAll("(a)(?:b|bc)+", "g", "aa", "[]");
testMatchAll("(?:AA|A)+", "g", "AABAAA", "[\"AA@0\",\"AAA@3\"]");

// lastIndex progression for global and sticky.
testLastIndex("(?:AA|A)+", "g", "AABAAAB", "[\"AA@0:2\",\"AAA@3:6\",\"null@0\"]");
testLastIndex("(a)(?:b|bc)+", "g", "abcbxabb", "[\"ab@0:2\",\"abb@5:8\",\"null@0\"]");
testLastIndex("(?:AA|A)+", "y", "AAB", "[\"AA@0:2\",\"null@0\"]");
testLastIndex("(?:a?)+", "g", "aab", "[\"aa@0:2\",\"@2:2\",\"@2:2\",\"@2:2\",\"@2:2\",\"@2:2\",\"@2:2\",\"@2:2\"]");
testLastIndex("(?:AA|A)*", "g", "AAB", "[\"AA@0:2\",\"@2:2\",\"@2:2\",\"@2:2\",\"@2:2\",\"@2:2\",\"@2:2\",\"@2:2\"]");
