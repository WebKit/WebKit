function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const trimStart = /^\s+/;
const trimEnd = /\s+$/;

function testStart(string) {
    return string.replace(trimStart, "");
}
noInline(testStart);

function testEnd(string) {
    return string.replace(trimEnd, "");
}
noInline(testEnd);

function testOther(string) {
    return string.replace(/Hello/, "");
}
noInline(testOther);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(testStart(" \t\n Hello"), "Hello");
    shouldBe(RegExp.input, " \t\n Hello");
    shouldBe(RegExp.leftContext, "");
    shouldBe(RegExp.lastMatch, " \t\n ");
    shouldBe(RegExp.rightContext, "Hello");

    shouldBe(testEnd("Hello \t\n "), "Hello");
    shouldBe(RegExp.input, "Hello \t\n ");
    shouldBe(RegExp.leftContext, "Hello");
    shouldBe(RegExp.lastMatch, " \t\n ");
    shouldBe(RegExp.rightContext, "");

    shouldBe(testStart("   "), "");
    shouldBe(RegExp.input, "   ");
    shouldBe(RegExp.lastMatch, "   ");

    shouldBe(testEnd("   "), "");
    shouldBe(RegExp.input, "   ");
    shouldBe(RegExp.lastMatch, "   ");

    shouldBe(testOther("errorHelloerror"), "errorerror");
    shouldBe(testStart("Hello   "), "Hello   ");
    shouldBe(testEnd("   Hello"), "   Hello");
    shouldBe(testStart(""), "");
    shouldBe(testEnd(""), "");
    shouldBe(RegExp.input, "errorHelloerror");
    shouldBe(RegExp.leftContext, "error");
    shouldBe(RegExp.rightContext, "error");
}
