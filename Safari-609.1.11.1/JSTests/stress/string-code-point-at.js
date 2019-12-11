function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function testSurrogatePair(testString, expected) {
    for (var i = 0; i < testString.length; ++i)
        shouldBe(testString.codePointAt(i), expected[i]);
}
noInline(testSurrogatePair);

for (var i = 0; i < 1e5; ++i) {
    var testString = 'Cocoa';
    var expected = [
        67,
        111,
        99,
        111,
        97,
    ];
    testSurrogatePair(testString, expected);

    // "\uD842\uDFB7\u91ce\u5bb6"
    var testString = "𠮷野家";
    var expected = [
        0x20BB7,
        0xDFB7,
        0x91CE,
        0x5BB6,
    ];
    testSurrogatePair(testString, expected);

    var testString = "A\uD842";
    var expected = [
        0x0041,
        0xD842,
    ];
    testSurrogatePair(testString, expected);

    var testString = "A\uD842A";
    var expected = [
        0x0041,
        0xD842,
        0x0041,
    ];
    testSurrogatePair(testString, expected);

    var testString = "A\uD842\uDFB7";
    var expected = [
        0x0041,
        0x20BB7,
        0xDFB7,
    ];
    testSurrogatePair(testString, expected);

    var testString = "\uD842A\uDFB7";
    var expected = [
        0xD842,
        0x0041,
        0xDFB7,
    ];
    testSurrogatePair(testString, expected);

    var testString = "\uDFB7\uD842A";
    var expected = [
        0xDFB7,
        0xD842,
        0x0041,
    ];
    testSurrogatePair(testString, expected);
}
