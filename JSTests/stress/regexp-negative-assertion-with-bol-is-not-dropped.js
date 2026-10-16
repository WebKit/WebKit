// A negative assertion whose content can only match at the start of the input fails there and
// succeeds everywhere else, so the once-through/loop split in optimizeBOL() must not drop it from
// the loop alternative. See copyTerm() in YarrPattern.cpp. The bug is in Yarr's pattern analysis,
// so it reproduces in every tier; the loop only makes sure the JIT tiers agree.

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
    check(/^a|(?!^)b/, "b", false, null);
    check(/^a|(?!^)b/, "xb", true, "b", 1);
    check(/^a|(?!^)b/, "ab", true, "a", 0);
    check(/(?!^)b|^a/, "b", false, null);
    check(/(?!^)b|^a/, "xb", true, "b", 1);
    shouldBe("ab".replace(/^x|(?!^)a/, "Z"), "ab", "replace at 0");
    shouldBe("ba".replace(/^x|(?!^)a/, "Z"), "bZ", "replace at 1");

    check(/^x|(?!^|zz)b/, "b", false, null);
    check(/^x|(?!^|zz)b/, "cb", true, "b", 1);
    check(/^x|(?!zz|^)b/, "b", false, null);
    check(/^x|(?!(?:^))b/, "b", false, null);
    check(/^x|(?!(?=^)|zz)b/, "b", false, null);

    check(/(?!(?=^)b)b/, "b", false, null);
    check(/(?!(?=^)b)b/, "xb", true, "b", 1);
    check(/(?!(?=^)b)c/, "bc", true, "c", 1);

    check(/^a|(?!\b^)b/, "b", false, null);
    check(/^a|(?!\b^)b/, " b", true, "b", 1);
}

for (var i = 0; i < testLoopCount; ++i)
    step();
