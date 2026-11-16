// Benchmark for String.prototype.startsWith constant folding

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

function testConstantOneChar() {
    return "Hello World".startsWith("H");
}
noInline(testConstantOneChar);

function testConstantOneCharWithIndex() {
    return "Hello World".startsWith("W", 6);
}
noInline(testConstantOneCharWithIndex);

for (var i = 0; i < 1e6; ++i) {
    testConstantFound();
    testConstantNotFound();
    testConstantWithIndex();
    testConstantOneChar();
    testConstantOneCharWithIndex();
}
