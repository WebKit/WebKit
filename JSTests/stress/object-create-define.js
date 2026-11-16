function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(prototype, data)
{
    return Object.create(prototype, data);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    var prototype = { Cocoa: false };
    var object = test(prototype, {
        Cappuccino: {
            value: 42,
            enumerable: true,
            configurable: true,
            writable: true
        },
        Matcha: {
            value: 40,
            enumerable: false,
            configurable: true,
            writable: true
        }
    });
    shouldBe(Object.getPrototypeOf(object), prototype);
    shouldBe(JSON.stringify(Object.getOwnPropertyNames(object).sort()), `["Cappuccino","Matcha"]`);
    shouldBe(JSON.stringify(Object.keys(object).sort()), `["Cappuccino"]`);
}
