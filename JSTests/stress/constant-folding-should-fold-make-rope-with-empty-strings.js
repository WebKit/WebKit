function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function unknown()
{
    return "OK";
}
noInline(unknown);

function readWord1(flag)
{
    var word = "";
    if (flag) {
        word += unknown();
    }
    return word + "HelloWorld";
}
noInline(readWord1);

function readWord2(flag)
{
    var word = "";
    if (flag) {
        word += unknown();
    }
    return "HelloWorld" + word;
}
noInline(readWord2);

function readWord3(flag)
{
    var word = "";
    if (flag) {
        word += unknown();
    }
    return "" + word;
}
noInline(readWord3);

function readWord4(flag)
{
    var word = "";
    if (flag) {
        word += unknown();
    }
    return "HelloWorld" + word + word;
}
noInline(readWord4);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(readWord1(false), "HelloWorld");
    shouldBe(readWord2(false), "HelloWorld");
    shouldBe(readWord3(false), "");
    shouldBe(readWord4(false), "HelloWorld");
}
shouldBe(readWord1(true), "OKHelloWorld");
shouldBe(readWord2(true), "HelloWorldOK");
shouldBe(readWord3(true), "OK");
shouldBe(readWord4(true), "HelloWorldOKOK");
