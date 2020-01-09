function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

 function test(array, flag)
{
    var iterator = array.values();
    var result = iterator.next().value;
    if (flag)
        return iterator;
    return result;
}
noInline(test);

 var array = [1, 2, 3];
var iterator = array.values();
var ArrayIteratorPrototype = iterator.__proto__;
for (var i = 0; i < 1e6; ++i) {
    var flag = i % 10000 === 0;
    var result = test(array, flag);
    if (flag) {
        shouldBe(typeof result, "object");
        shouldBe(result.next().value, 2);
        shouldBe(result.__proto__, ArrayIteratorPrototype);
    } else
        shouldBe(result, 1);
}