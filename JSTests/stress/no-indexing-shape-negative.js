function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(object, index) {
    return object[index];
}
noInline(test);

Object.prototype[-30] = 42;
var object = {};
for (var i = 0; i < 1e3; ++i)
    shouldBe(test(object, i), undefined);
shouldBe(test(object, -30), 42);
