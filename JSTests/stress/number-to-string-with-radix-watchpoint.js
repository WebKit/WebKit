function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    for (var i = 0; i < 10; ++i) {
        var result = '';
        result += i.toString(2);
        result += i.toString(4);
        result += i.toString(8);
        result += i.toString(16);
        result += i.toString(32);
    }
    return result;
}
noInline(test);

var result = `1001211199`;
for (var i = 0; i < 1e4; ++i) {
    if (i === 1e3) {
        Number.prototype.toString = function (radix) { return "Hello"; }
        result = `HelloHelloHelloHelloHello`;
    }
    shouldBe(test(), result);
}
