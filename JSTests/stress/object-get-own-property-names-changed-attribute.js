function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(object)
{
    return Object.getOwnPropertyNames(object);
}
noInline(test);

var object = { Cocoa: 42 };
for (var i = 0; i < 1e6; ++i) {
    var result = test(object);
    shouldBe(result.length, 1);
    shouldBe(result[0], 'Cocoa');
}

Reflect.defineProperty(object, 'Cocoa', {
    enumerable: false
});

for (var i = 0; i < 1e6; ++i) {
    var result = test(object);
    shouldBe(result.length, 1);
    shouldBe(result[0], 'Cocoa');
}
