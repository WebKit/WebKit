function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function testConstantFound() {
    return "Hello World".startsWith("Hello");
}
noInline(testConstantFound);

function testConstantNotFound() {
    return "Hello World".startsWith("World");
}
noInline(testConstantNotFound);

function testConstantWithIndex() {
    return "Hello World".startsWith("World", 6);
}
noInline(testConstantWithIndex);

function testConstantWithIndexNotFound() {
    return "Hello World".startsWith("World", 7);
}
noInline(testConstantWithIndexNotFound);

function testConstantOneChar() {
    return "Hello World".startsWith("H");
}
noInline(testConstantOneChar);

function testConstantOneCharNotFound() {
    return "Hello World".startsWith("W");
}
noInline(testConstantOneCharNotFound);

function testConstantOneCharWithIndex() {
    return "Hello World".startsWith("W", 6);
}
noInline(testConstantOneCharWithIndex);

function testConstantOneCharWithIndexNotFound() {
    return "Hello World".startsWith("W", 7);
}
noInline(testConstantOneCharWithIndexNotFound);

function testEmptySearch() {
    return "Hello World".startsWith("");
}
noInline(testEmptySearch);

function testEmptySearchWithIndex() {
    return "Hello World".startsWith("", 5);
}
noInline(testEmptySearchWithIndex);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(testConstantFound(), true);
    shouldBe(testConstantNotFound(), false);
    shouldBe(testConstantWithIndex(), true);
    shouldBe(testConstantWithIndexNotFound(), false);
    shouldBe(testConstantOneChar(), true);
    shouldBe(testConstantOneCharNotFound(), false);
    shouldBe(testConstantOneCharWithIndex(), true);
    shouldBe(testConstantOneCharWithIndexNotFound(), false);
    shouldBe(testEmptySearch(), true);
    shouldBe(testEmptySearchWithIndex(), true);
}
