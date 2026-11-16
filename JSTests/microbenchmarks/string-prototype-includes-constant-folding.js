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

function testConstantOneChar() {
    return "Hello World".includes("W");
}
noInline(testConstantOneChar);

function testConstantOneCharWithIndex() {
    return "Hello World".includes("W", 6);
}
noInline(testConstantOneCharWithIndex);

for (var i = 0; i < 1e6; ++i) {
    testConstantFound();
    testConstantNotFound();
    testConstantWithIndex();
    testConstantOneChar();
    testConstantOneCharWithIndex();
}
