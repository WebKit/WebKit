function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

 function test(set, flag)
{
    var iterator = set.values();
    var result = iterator.next().value;
    if (flag)
        return iterator;
    return result;
}
noInline(test);

var set = new Set([1, 2, 3]);
var iterator = set.values();
var setIteratorPrototype = iterator.__proto__;
for (var i = 0; i < 1e6; ++i) {
    var flag = i % 10000 === 0;
    var result = test(set, flag);
    if (flag) {
        shouldBe(typeof result, "object");
        shouldBe(result.next().value, 2);
        shouldBe(result.__proto__, setIteratorPrototype);
    } else
        shouldBe(result, 1);
}
