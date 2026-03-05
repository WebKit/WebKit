function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function testConstantFound() {
    return "Hello World".includes("World");
}
noInline(testConstantFound);

function testConstantNotFound() {
    return "Hello World".includes("xyz");
}
noInline(testConstantNotFound);

function testConstantWithIndex() {
    return "Hello World".includes("World", 6);
}
noInline(testConstantWithIndex);

function testConstantWithIndexNotFound() {
    return "Hello World".includes("World", 7);
}
noInline(testConstantWithIndexNotFound);

function testConstantOneChar() {
    return "Hello World".includes("W");
}
noInline(testConstantOneChar);

function testConstantOneCharNotFound() {
    return "Hello World".includes("X");
}
noInline(testConstantOneCharNotFound);

function testConstantOneCharWithIndex() {
    return "Hello World".includes("W", 6);
}
noInline(testConstantOneCharWithIndex);

function testConstantOneCharWithIndexNotFound() {
    return "Hello World".includes("W", 7);
}
noInline(testConstantOneCharWithIndexNotFound);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(testConstantFound(), true);
    shouldBe(testConstantNotFound(), false);
    shouldBe(testConstantWithIndex(), true);
    shouldBe(testConstantWithIndexNotFound(), false);
    shouldBe(testConstantOneChar(), true);
    shouldBe(testConstantOneCharNotFound(), false);
    shouldBe(testConstantOneCharWithIndex(), true);
    shouldBe(testConstantOneCharWithIndexNotFound(), false);
}
