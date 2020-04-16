function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

 function test(map, flag)
{
    var iterator = map.values();
    var result = iterator.next().value;
    if (flag)
        return iterator;
    return result;
}
noInline(test);

var map = new Map([[1, 1], [2, 2], [3, 3]]);
var iterator = map.values();
var mapIteratorPrototype = iterator.__proto__;
for (var i = 0; i < 1e6; ++i) {
    var flag = i % 10000 === 0;
    var result = test(map, flag);
    if (flag) {
        shouldBe(typeof result, "object");
        shouldBe(result.next().value, 2);
        shouldBe(result.__proto__, mapIteratorPrototype);
    } else
        shouldBe(result, 1);
}
