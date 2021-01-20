function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1(object)
{
    return "customValue" in object;
}
noInline(test1);

function test2(object)
{
    return "customAccessor" in object;
}
noInline(test2);

var target1 = $vm.createCustomTestGetterSetter();
var target2 = { __proto__: target1 };
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test1(target1), true);
    shouldBe(test1(target2), true);
    shouldBe(test2(target1), true);
    shouldBe(test2(target2), true);
}
