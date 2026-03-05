function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function test(string, search) {
    return string.endsWith(search);
}
noInline(test);

function testWithEndPosition(string, search, endPosition) {
    return string.endsWith(search, endPosition);
}
noInline(testWithEndPosition);

function makeString(base) {
    return base + "";
}
noInline(makeString);

// Basic Unicode characters
var unicodeString = makeString("こんにちは世界");
var unicodeSearch1 = makeString("世界");
var unicodeSearch2 = makeString("こんにちは");
var unicodeSearch3 = makeString("界");

// Surrogate pairs (emoji)
var emojiString = makeString("Hello😀🎉World🌍");
var emojiSearch1 = makeString("🌍");
var emojiSearch2 = makeString("World🌍");
var emojiSearch3 = makeString("😀");
var emojiSearch4 = makeString("🎉World🌍");

// Mixed ASCII and Unicode
var mixedString = makeString("Hello世界こんにちは");
var mixedSearch1 = makeString("こんにちは");
var mixedSearch2 = makeString("世界こんにちは");
var mixedSearch3 = makeString("世界");

for (var i = 0; i < testLoopCount; ++i) {
    // Basic Unicode tests
    shouldBe(test(unicodeString, unicodeSearch1), true);
    shouldBe(test(unicodeString, unicodeSearch2), false);
    shouldBe(test(unicodeString, unicodeSearch3), true);
    shouldBe(test(unicodeString, makeString("")), true);

    // Surrogate pair tests
    shouldBe(test(emojiString, emojiSearch1), true);
    shouldBe(test(emojiString, emojiSearch2), true);
    shouldBe(test(emojiString, emojiSearch3), false);
    shouldBe(test(emojiString, emojiSearch4), true);

    // Mixed ASCII and Unicode tests
    shouldBe(test(mixedString, mixedSearch1), true);
    shouldBe(test(mixedString, mixedSearch2), true);
    shouldBe(test(mixedString, mixedSearch3), false);

    // Unicode with endPosition
    shouldBe(testWithEndPosition(unicodeString, makeString("こんにちは"), 5), true);
    shouldBe(testWithEndPosition(unicodeString, makeString("こんにちは"), 6), false);

    // Emoji with endPosition (note: emoji are 2 UTF-16 code units each)
    shouldBe(testWithEndPosition(emojiString, makeString("😀"), 7), true);
    shouldBe(testWithEndPosition(emojiString, makeString("🎉"), 9), true);
    shouldBe(testWithEndPosition(emojiString, makeString("Hello"), 5), true);
}
