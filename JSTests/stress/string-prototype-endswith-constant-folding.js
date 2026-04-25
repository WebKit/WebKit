function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function testConstantFound() {
    return "Hello World".endsWith("World");
}
noInline(testConstantFound);

function testConstantNotFound() {
    return "Hello World".endsWith("Hello");
}
noInline(testConstantNotFound);

function testConstantWithEndPosition() {
    return "Hello World".endsWith("Hello", 5);
}
noInline(testConstantWithEndPosition);

function testConstantWithEndPositionNotFound() {
    return "Hello World".endsWith("Hello", 4);
}
noInline(testConstantWithEndPositionNotFound);

function testConstantOneChar() {
    return "Hello World".endsWith("d");
}
noInline(testConstantOneChar);

function testConstantOneCharNotFound() {
    return "Hello World".endsWith("o");
}
noInline(testConstantOneCharNotFound);

function testConstantOneCharWithEndPosition() {
    return "Hello World".endsWith("o", 5);
}
noInline(testConstantOneCharWithEndPosition);

function testConstantOneCharWithEndPositionNotFound() {
    return "Hello World".endsWith("o", 4);
}
noInline(testConstantOneCharWithEndPositionNotFound);

function testEmptySearch() {
    return "Hello World".endsWith("");
}
noInline(testEmptySearch);

function testEmptySearchWithEndPosition() {
    return "Hello World".endsWith("", 5);
}
noInline(testEmptySearchWithEndPosition);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(testConstantFound(), true);
    shouldBe(testConstantNotFound(), false);
    shouldBe(testConstantWithEndPosition(), true);
    shouldBe(testConstantWithEndPositionNotFound(), false);
    shouldBe(testConstantOneChar(), true);
    shouldBe(testConstantOneCharNotFound(), false);
    shouldBe(testConstantOneCharWithEndPosition(), true);
    shouldBe(testConstantOneCharWithEndPositionNotFound(), false);
    shouldBe(testEmptySearch(), true);
    shouldBe(testEmptySearchWithEndPosition(), true);
}
