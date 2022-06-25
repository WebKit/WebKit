function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

 function test(map)
{
    var iterator = map.values();
    return iterator.next().value;
}
noInline(test);

var map = new Map([[1, 1], [2, 2], [3, 3]]);
for (var i = 0; i < 1e6; ++i)
    shouldBe(test(map), 1);
