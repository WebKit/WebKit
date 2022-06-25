function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(prototype)
{
    return Object.create(prototype);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    var prototype = { Cocoa: false };
    var object = test(prototype);
    shouldBe(Object.getPrototypeOf(object), prototype);
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(object)), `[]`);
}
