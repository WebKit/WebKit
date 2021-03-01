function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = new Int32Array(4);

array.fill(2);
for (var index = 0; index < array.length; ++index)
    shouldBe(array[index], 2);

array.fill(3, 2);
for (var index = 0; index < 2; ++index)
    shouldBe(array[index], 2);
for (var index = 2; index < array.length; ++index)
    shouldBe(array[index], 3);

array.fill(4, 2, 3);
for (var index = 0; index < 2; ++index)
    shouldBe(array[index], 2);
shouldBe(array[2], 4);
shouldBe(array[3], 3);

array.fill(5, 0, -2);
for (var index = 0; index < 2; ++index)
    shouldBe(array[index], 5);
shouldBe(array[2], 4);
shouldBe(array[3], 3);
