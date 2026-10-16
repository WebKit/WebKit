function shouldBe(actual, expected, context) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error("bad value: " + JSON.stringify(actual) + " expected: " + JSON.stringify(expected) + " for " + context);
}

function check(re, string, expected, expectedIndex) {
    const context = re + " against " + JSON.stringify(string) + " from lastIndex " + re.lastIndex;
    const result = re.exec(string);
    shouldBe(result === null ? null : Array.from(result, (x) => x === undefined ? null : x), expected, context);
    if (result !== null)
        shouldBe(result.index, expectedIndex, context + " (index)");
}

let re;

re = /a|^x|aa/g;
re.lastIndex = 2;
check(re, "xxcaa", ["a"], 3);
shouldBe(re.lastIndex, 4, "/a|^x|aa/g lastIndex after match");
re = /a|^x|aa/y;
re.lastIndex = 1;
check(re, "caa", ["a"], 1);
re = /a|^x|aa/y;
re.lastIndex = 0;
check(re, "caa", null);

check(/a|^b|c|^d|ab|^e/, "xab", ["a"], 1);
check(/a|aa|^x/, "caa", ["a"], 1);
check(/ab|a|^x|abc/, "cabc", ["ab"], 1);
check(/aaaa|^x|a/, "ca", ["a"], 1);
check(/a|^x|aa|(?:^y|b)/, "caa", ["a"], 1);
check(/b|^x|(?:^y|a)|aa/, "caa", ["a"], 1);
check(/(?:a|^x|aa)/, "caa", ["a"], 1);

check(/(a)|^(b)|(a)a/, "caa", ["a", "a", null, null], 1);
check(/(?<n>a)|^x|(?<n>a)a/, "caa", ["a", "a", null], 1);
shouldBe("caab".replace(/a|^x|(a+)b/, "[$&,$1]"), "c[a,]ab", "replace with capture");

check(/a|(?:^x)|aa/, "caa", ["a"], 1);
check(/a|(^)x|aa/, "caa", ["a", null], 1);
check(/a|(?:^x)?b|ab/, "cab", ["a"], 1);
check(/a|(?=^)x|aa/, "caa", ["a"], 1);
check(/a|(?<=^)x|aa/, "caa", ["a"], 1);
check(/a|(?<=^c)a|aa|^x/, "caa", ["a"], 1);
check(/(?<=c)a|^x|aa/, "caa", ["a"], 1);
check(/(?<!c)a|^x|a+/, "caa", ["aa"], 1);
check(/b(?<=(a)b)|^x|(b)c/, "abc", ["b", "a", null], 1);
check(/a(?!a)|^x|a+/, "caa", ["aa"], 1);
check(/a\b|^x|a\B./, "caa", ["aa"], 1);
check(/\ba|^x|a/, "c a", ["a"], 2);
check(/a$|^x|aa$/, "caa", ["aa"], 1);

check(/|^x|a/, "ca", [""], 0);
check(/a|^x|/, "ca", [""], 0);

check(/A|^X|AA/i, "caa", ["a"], 1);
check(/b.|^x|b/s, "ab\n", ["b\n"], 1);
check(/b.|^x|b/, "ab\n", ["b"], 1);
check(/a|^x|aa/m, "c\nxaa", ["x"], 2);

check(/b|^x|bb/u, "\u{1F600}bb", ["b"], 2);
check(/\u{1F600}|^x|\u{1F600}\u{1F600}/u, "a\u{1F600}\u{1F600}", ["\u{1F600}"], 1);
check(/\uD83D|^x|\uD83D\uDE00/, "a\uD83D\uDE00", ["\uD83D"], 1);

shouldBe("caa xaa".split(/a|^x|aa/), ["c", "", " x", "", ""], "split");
shouldBe(Array.from("caa".matchAll(/a|^x|(a)a/g), (m) => m[0]), ["a", "a"], "matchAll");
shouldBe("caa xaa".replace(/a|^x|aa/g, "_"), "c__ x__", "replace /g");
