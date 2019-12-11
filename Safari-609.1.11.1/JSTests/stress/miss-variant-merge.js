function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(object)
{
    return object.return;
}
noInline(test);

var object1 = {};
var object2 = { hello: 42 };
for (var i = 0; i < 10; ++i) {
    shouldBe(test(object1), undefined);
}
for (var i = 0; i < 1e6; ++i) {
    shouldBe(test(object1), undefined);
    shouldBe(test(object2), undefined);
}
