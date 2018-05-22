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

var protoGetter = {
    __proto__: {
        get hello() {
            throw new Error("out");
        }
    }
};
var protoSetter = {
    __proto__: {
        set hello(value) {
            throw new Error("out");
        }
    }
};
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test({
        get hello() {
            throw new Error("out");
        }
    }), true);
    shouldBe(test({}), false);
    shouldBe(test(protoGetter), true);
    shouldBe(test({
        set hello(value) {
            throw new Error("out");
        }
    }), true);
    shouldBe(test(protoSetter), true);
}
