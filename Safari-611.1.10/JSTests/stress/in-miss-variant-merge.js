function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(object)
{
    return 'return' in object;
}
noInline(test);

var object1 = {};
var object2 = { hello: 42 };
for (var i = 0; i < 10; ++i) {
    shouldBe(test(object1), false);
}
for (var i = 0; i < 1e6; ++i) {
    shouldBe(test(object1), false);
    shouldBe(test(object2), false);
}
