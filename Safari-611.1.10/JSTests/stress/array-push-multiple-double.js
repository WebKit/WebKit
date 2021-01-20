function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array, val1, val2, val3)
{
    return array.push(val1, val2, val3);
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    var array = [];
    shouldBe(test(array, 1.1, 2.2, 3.3), 3);
    shouldBe(array[0], 1.1);
    shouldBe(array[1], 2.2);
    shouldBe(array[2], 3.3);
}

for (var i = 0; i < 1e5; ++i) {
    var array = [];
    shouldBe(test(array, 1.1, 2.2, 4), 3);
    shouldBe(array[0], 1.1);
    shouldBe(array[1], 2.2);
    shouldBe(array[2], 4);
}
var array = [];
shouldBe(test(array, 1.1, 2.2, "String"), 3);
shouldBe(array[0], 1.1);
shouldBe(array[1], 2.2);
shouldBe(array[2], "String");
