function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

const words = [];
for (let i = 0; i < 300; ++i)
    words.push("w" + i.toString(36) + (i % 7 ? "" : "x"));
const fixedWidthWords = [];
for (let i = 0; i < 300; ++i)
    fixedWidthWords.push("w" + i.toString(36).padStart(2, "0"));

const stringList = new RegExp("^(" + words.join("|") + ")$");
const nested = new RegExp("^(" + words.map((word) => "(?:" + word + "|" + word + "y)z?").join("|") + ")-(a|b|c)+$");
const capturing = new RegExp("(" + words.join("|") + ")-(\\d*)");
const fixedCount = new RegExp("^(?:" + words.join("|") + "){2}$");
const lookahead = new RegExp("^(?=(?:" + words.join("|") + ")-)w");
const negativeLookahead = new RegExp("^(?!(?:" + words.join("|") + ")$)w");
const lookbehind = new RegExp("(?<=" + fixedWidthWords.join("|") + ")-(\\d+)");
const negativeLookbehind = new RegExp("(?<!" + fixedWidthWords.join("|") + ")-(\\d+)");

for (let i = 0; i < testLoopCount; ++i) {
    const word = words[i % words.length];
    const otherWord = words[(i * 7 + 1) % words.length];
    const fixedWidthWord = fixedWidthWords[i % fixedWidthWords.length];
    shouldBe(stringList.test(word), true);
    shouldBe(stringList.test(word + "!"), false);
    shouldBe(nested.test(word + "yz-abca"), true);
    shouldBe(nested.test(word + "y-"), false);
    const match = capturing.exec("--" + word + "-123");
    shouldBe(match[1], word);
    shouldBe(match[2], "123");
    shouldBe(fixedCount.test(word + otherWord), true);
    shouldBe(fixedCount.test(word + otherWord + "!"), false);
    shouldBe(lookahead.test(word + "-"), true);
    shouldBe(lookahead.test(word + "!"), false);
    shouldBe(negativeLookahead.test(word), false);
    shouldBe(negativeLookahead.test(word + "!"), true);
    shouldBe(lookbehind.exec("zz" + fixedWidthWord + "-42")[1], "42");
    shouldBe(lookbehind.exec("zzabc-42"), null);
    shouldBe(negativeLookbehind.exec("zz" + fixedWidthWord + "-42"), null);
    shouldBe(negativeLookbehind.exec("zzabc-42")[1], "42");
}
