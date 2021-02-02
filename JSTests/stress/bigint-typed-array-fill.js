function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = new BigInt64Array(4);

array.fill(2n);
for (var index = 0; index < array.length; ++index)
    shouldBe(array[index], 2n);

array.fill(3n, 2);
for (var index = 0; index < 2; ++index)
    shouldBe(array[index], 2n);
for (var index = 2; index < array.length; ++index)
    shouldBe(array[index], 3n);

array.fill(4n, 2, 3);
for (var index = 0; index < 2; ++index)
    shouldBe(array[index], 2n);
shouldBe(array[2], 4n);
shouldBe(array[3], 3n);

array.fill(5n, 0, -2);
for (var index = 0; index < 2; ++index)
    shouldBe(array[index], 5n);
shouldBe(array[2], 4n);
shouldBe(array[3], 3n);
