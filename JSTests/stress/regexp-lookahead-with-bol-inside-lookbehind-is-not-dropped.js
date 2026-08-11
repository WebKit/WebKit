// A lookbehind walks back from wherever the outer match starts, so a ^ inside it can reach the start
// of the input even when the match itself does not start there. The once-through/loop split in
// optimizeBOL() must therefore not filter ^-anchored alternatives out of anything nested in a
// lookbehind, including a lookahead, which is parsed as Forward again. See copyTerm() in
// YarrPattern.cpp. The bug is in Yarr's pattern analysis, so it reproduces in every tier; the loop
// only makes sure the JIT tiers agree.

function shouldBe(actual, expected, context) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected + " for " + context);
}

function check(re, string, expectedTest, expectedMatch, expectedIndex) {
    const context = re + " against " + JSON.stringify(string);
    shouldBe(re.test(string), expectedTest, context);
    const result = re.exec(string);
    if (expectedMatch === null) {
        shouldBe(result, null, context);
        return;
    }
    shouldBe(result[0], expectedMatch, context);
    shouldBe(result.index, expectedIndex, context);
}

function step() {
    check(/(?<=(?=^)a)b|c/, "ab", true, "b", 1);
    check(/(?<=(?=^)a)b|c/, "abc", true, "b", 1);
    check(/(?<=(?=^)a)b|c/, "xab", false, null);
    check(/(?<=(?=^)a)b|c/, "xabc", true, "c", 3);
    check(/(?<=(?=^)a)b|c/, "b", false, null);
    check(/c|(?<=(?=^)a)b/, "ab", true, "b", 1);
    check(/(?<=(?:(?=^))a)b|c/, "ab", true, "b", 1);
    check(/(?<=((?=^)a))b|c/, "ab", true, "b", 1);
    check(/(?<=(?=^)aa)b|c/, "aab", true, "b", 2);
    check(/(?<=(?=^|zz)a)b|c/, "ab", true, "b", 1);
    shouldBe("ab".search(/(?<=(?=^)a)b|c/), 1, "search");
    shouldBe("ab".replace(/(?<=(?=^)a)b|c/, "Z"), "aZ", "replace at 1");
    shouldBe("cab".replace(/(?<=(?=^)a)b|c/, "Z"), "Zab", "replace at 0");
    shouldBe(JSON.stringify("ab ab".match(/(?<=(?=^)a)b|c/g)), '["b"]', "match global");
}

for (var i = 0; i < testLoopCount; ++i)
    step();
