function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(object)
{
    var name = "hello";
    return name in object;
}
noInline(test);

var proto = {
    __proto__: { hello: 2 }
};
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test({
        hello: 42
    }), true);
    shouldBe(test({}), false);
    shouldBe(test({
        helloworld: 43,
        ok: 44
    }), false);
    shouldBe(test(proto), true);
}
