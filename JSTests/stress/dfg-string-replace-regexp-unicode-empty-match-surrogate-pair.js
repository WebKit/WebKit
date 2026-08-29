function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${JSON.stringify(actual)}, expected ${JSON.stringify(expected)}`);
}

function replaceUnicode() {
    return "a\u{1F600}b".replace(/(?:)/gu, "-");
}
noInline(replaceUnicode);

function replaceUnicodeSets() {
    return "a\u{1F600}b".replace(/(?:)/gv, "-");
}
noInline(replaceUnicodeSets);

function replaceAllUnicode() {
    return "\u{1F600}\u{1F601}".replaceAll(/(?:)/gu, "|");
}
noInline(replaceAllUnicode);

function replaceLoneLead() {
    return "a\uD83D".replace(/(?:)/gu, "-");
}
noInline(replaceLoneLead);

function replaceNonUnicode() {
    return "a\u{1F600}b".replace(/(?:)/g, "-");
}
noInline(replaceNonUnicode);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(replaceUnicode(), "-a-\u{1F600}-b-");
    shouldBe(replaceUnicodeSets(), "-a-\u{1F600}-b-");
    shouldBe(replaceAllUnicode(), "|\u{1F600}|\u{1F601}|");
    shouldBe(replaceLoneLead(), "-a-\uD83D-");
    shouldBe(replaceNonUnicode(), "-a-\uD83D-\uDE00-b-");
}
