function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(object)
{
    return "$1" in object;
}
noInline(test);

var target1 = RegExp;
var target2 = { __proto__: RegExp };
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test(target1), true);
    shouldBe(test(target2), true);
}
