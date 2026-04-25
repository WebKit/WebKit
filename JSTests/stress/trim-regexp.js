function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const trimStart = /^\s+/;
const trimEnd = /\s+$/;

{
    let result = "    Hello".replace(trimStart, "");
    shouldBe(result, "Hello");
    shouldBe(RegExp.input, "    Hello");
    shouldBe(RegExp.leftContext, "");
    shouldBe(RegExp.rightContext, "Hello");
}
{
    let result = "Hello    ".replace(trimEnd, "");
    shouldBe(result, "Hello");
    shouldBe(RegExp.input, "Hello    ");
    shouldBe(RegExp.leftContext, "Hello");
    shouldBe(RegExp.rightContext, "");
}
"errorHelloerror".replace(/Hello/, "");
{
    let result = "Hello".replace(trimStart, "");
    shouldBe(result, "Hello");
    shouldBe(RegExp.input, "errorHelloerror");
    shouldBe(RegExp.leftContext, "error");
    shouldBe(RegExp.rightContext, "error");
}
{
    let result = "Hello".replace(trimEnd, "");
    shouldBe(result, "Hello");
    shouldBe(RegExp.input, "errorHelloerror");
    shouldBe(RegExp.leftContext, "error");
    shouldBe(RegExp.rightContext, "error");
}
