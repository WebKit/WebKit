function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array1) {
    return array1.concat();
}
noInline(test);

var obj1 = {}, obj2 = {}, obj3 = {}, obj4 = {};
var array1 = [obj1, obj2, obj3, obj4];
for (var i = 0; i < 1e4; ++i) {
    var result = test(array1);
    shouldBe(result[0], obj1);
    shouldBe(result[1], obj2);
    shouldBe(result[2], obj3);
    shouldBe(result[3], obj4);
    shouldBe(result.length, 4);
}
