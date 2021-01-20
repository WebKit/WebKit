function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

 function test(set, flag)
{
    var iterator = set.values();
    iterator.next();
    if (flag) {
        OSRExit();
        return iterator;
    }
    return iterator.next().done;
}
noInline(test);

var set = new Set([0]);
for (var i = 0; i < 1e5; ++i)
    shouldBe(test(set, false), true);

 var iterator = test(set, true);
shouldBe(iterator.__proto__, (new Set).values().__proto__);
shouldBe(iterator.next().done, true);
