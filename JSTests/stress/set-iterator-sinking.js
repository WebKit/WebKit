function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

 function test(set)
{
    var iterator = set.values();
    return iterator.next().value;
}
noInline(test);

var set = new Set([1, 2, 3]);
for (var i = 0; i < 1e6; ++i)
    shouldBe(test(set), 1);
