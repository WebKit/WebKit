function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1(string)
{
    return string.toString();
}
noInline(test1);

function test2(string)
{
    return string.toString();
}
noInline(test2);

function test3(string)
{
    return string.toString();
}
noInline(test3);

var string = "Hello";
var stringObject = new String(string);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(test1(string), string);
    shouldBe(test2(stringObject), string);
    if (i & 1)
        shouldBe(test3(string), string);
    else
        shouldBe(test3(stringObject), string);
}

shouldBe(test1({}), `[object Object]`);
shouldBe(test2({}), `[object Object]`);
shouldBe(test3({}), `[object Object]`);
