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

check(/^a/, "ba", null);
check(/^a/, "", null);
check(/^a|^b/, "xab", null);
check(/^a|^b/, "b", ["b"], 0);
check(/^a|^b/, "", null);
check(/^(a)|^b/, "xb", null);
check(/(?:^a)+|^b/, "xab", null);
check(/(^a|^c)|^b/, "xab", null);
check(/(?=^)a|^b/, "xa", null);
check(/(?=^a)\w|^b/, "xa", null);

check(/^A|^B/i, "xab", null);
check(/^.a|^b/s, "x\nab", null);
check(/^a|^b/u, "\u{1F600}a", null);
check(/^a|^b/m, "x\nb", ["b"], 2);

re = /^a|^b/g;
re.lastIndex = 1;
check(re, "aab", null);
shouldBe(re.lastIndex, 0, "lastIndex reset after failing from lastIndex 1");
re = /^a|^b/y;
re.lastIndex = 1;
check(re, "xab", null);
shouldBe("abab".match(/^a|^b/g), ["a"], "match /g");
shouldBe("bab".replace(/^b|^a/g, "_"), "_ab", "replace /g");
shouldBe("aXbX".split(/^a|^b/), ["", "XbX"], "split");

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(/^a/.test("b".repeat(i & 0xff)), false, "/^a/ long input");
    shouldBe(/^a|^b(?<!c)/.exec("x".repeat(i & 0xff) + "ab"), (i & 0xff) ? null : ["a"], "/^a|^b(?<!c)/ long input");
}
