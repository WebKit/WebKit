function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1(object, key)
{
    return delete object[key];
}
noInline(test1);

function test2(object, key)
{
    return delete object[key];
}
noInline(test2);

for (var i = 0; i < 1e5; ++i) {
    var object = {
        cocoa: 42
    };
    Object.defineProperty(object, "cappuccino", {
        value: 42
    });
    shouldBe(test1(object, "cocoa"), true);
    shouldBe(test2(object, "cappuccino"), false);
}
