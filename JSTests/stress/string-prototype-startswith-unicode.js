function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function test(string, search) {
    return string.startsWith(search);
}
noInline(test);

function testWithIndex(string, search, index) {
    return string.startsWith(search, index);
}
noInline(testWithIndex);

function makeString(base) {
    return base + "";
}
noInline(makeString);

// Basic Unicode characters
var unicodeString = makeString("こんにちは世界");
var unicodeSearch1 = makeString("こんにちは");
var unicodeSearch2 = makeString("世界");
var unicodeSearch3 = makeString("こん");

// Surrogate pairs (emoji)
var emojiString = makeString("😀🎉Hello🌍World");
var emojiSearch1 = makeString("😀");
var emojiSearch2 = makeString("😀🎉");
var emojiSearch3 = makeString("🎉");
var emojiSearch4 = makeString("😀🎉Hello");

// Mixed ASCII and Unicode
var mixedString = makeString("Hello世界こんにちは");
var mixedSearch1 = makeString("Hello");
var mixedSearch2 = makeString("Hello世界");
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

    // Unicode with index
    shouldBe(testWithIndex(unicodeString, makeString("世界"), 5), true);
    shouldBe(testWithIndex(unicodeString, makeString("世界"), 4), false);

    // Emoji with index (note: emoji are 2 UTF-16 code units each)
    shouldBe(testWithIndex(emojiString, makeString("🎉"), 2), true);
    shouldBe(testWithIndex(emojiString, makeString("Hello"), 4), true);
    shouldBe(testWithIndex(emojiString, makeString("🌍"), 9), true);
}
