function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    return Object.create(null);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    var object = test();
    shouldBe(Object.getPrototypeOf(object), null);
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(object)), `[]`);
}
