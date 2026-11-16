// Benchmark for String.prototype.endsWith constant folding

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

function testConstantOneChar() {
    return "Hello World".endsWith("d");
}
noInline(testConstantOneChar);

function testConstantOneCharWithEndPosition() {
    return "Hello World".endsWith("o", 5);
}
noInline(testConstantOneCharWithEndPosition);

for (var i = 0; i < 1e6; ++i) {
    testConstantFound();
    testConstantNotFound();
    testConstantWithEndPosition();
    testConstantOneChar();
    testConstantOneCharWithEndPosition();
}
