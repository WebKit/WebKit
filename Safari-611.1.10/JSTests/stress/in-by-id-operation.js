function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(object)
{
    return "hello" in object;
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(test({
        hello: 42
    }), true);
    shouldBe(test({
        hello: undefined,
        world: 44
    }), true);
    shouldBe(test({
        helloworld: 43,
        ok: 44
    }), false);
}

function selfCache(object)
{
    return "hello" in object;
}
noInline(selfCache);

var object = {};
object.hello = 42;
for (var i = 0; i < 1e6; ++i)
    shouldBe(selfCache(object), true);
object.world = 43;
shouldBe(selfCache(object), true);
object.world = 43;
shouldBe(selfCache({ __proto__: object }), true);
delete object.hello;
shouldBe(selfCache(object), false);
shouldBe(selfCache({ __proto__: object }), false);
