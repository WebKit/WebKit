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

var object = {};
for (var i = 0; i < 1e5; ++i)
    shouldBe(test(object), `[object Object]`);
Object.prototype[Symbol.toStringTag] = "Hello";
shouldBe(test(object), `[object Hello]`);
