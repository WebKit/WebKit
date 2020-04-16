function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

 function test(map, flag)
{
    var iterator = map.values();
    iterator.next();
    if (flag) {
        OSRExit();
        return iterator;
    }
    return iterator.next().done;
}
noInline(test);

var map = new Map([[0, 0]]);
for (var i = 0; i < 1e5; ++i)
    shouldBe(test(map, false), true);

 var iterator = test(map, true);
shouldBe(iterator.__proto__, (new Map()).values().__proto__);
shouldBe(iterator.next().done, true);
