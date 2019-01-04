function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(value)
{
    return Object.prototype.toString.call(value);
}
noInline(test);

var value0 = {};
var value1 = { [Symbol.toStringTag]: "Hello" };
var value2 = new Date();
var value3 = "Hello";
var value4 = 42;
var value5 = Symbol("Cocoa");
var value6 = 42.195;
var value7 = false;

for (var i = 0; i < 1e6; ++i) {
    switch (i % 8) {
    case 0:
        shouldBe(test(value0), `[object Object]`);
        break;
    case 1:
        shouldBe(test(value1), `[object Hello]`);
        break;
    case 2:
        shouldBe(test(value2), `[object Date]`);
        break;
    case 3:
        shouldBe(test(value3), `[object String]`);
        break;
    case 4:
        shouldBe(test(value4), `[object Number]`);
        break;
    case 5:
        shouldBe(test(value5), `[object Symbol]`);
        break;
    case 6:
        shouldBe(test(value6), `[object Number]`);
        break;
    case 7:
        shouldBe(test(value7), `[object Boolean]`);
        break;
    }
}
