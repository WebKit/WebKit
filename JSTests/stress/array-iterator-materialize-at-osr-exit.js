function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

 function test(array, flag)
{
    var iterator = array.values();
    iterator.next();
    if (flag) {
        OSRExit();
        return iterator;
    }
    return iterator.next().done;
}
noInline(test);

 var array = [0];
for (var i = 0; i < 1e5; ++i)
    shouldBe(test(array, false), true);

 var iterator = test(array, true);
shouldBe(iterator.__proto__, [].values().__proto__);
shouldBe(iterator.next().done, true);