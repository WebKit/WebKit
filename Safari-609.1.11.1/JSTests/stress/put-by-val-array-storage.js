function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testArrayStorageInBounds(array, index, value)
{
    array[index] = value;
}
noInline(testArrayStorageInBounds);

for (var i = 0; i < 1e5; ++i) {
    var array = [1, 2, 3, 4, 5];
    ensureArrayStorage(array);
    shouldBe(array[0], 1);
    testArrayStorageInBounds(array, 0, 42);
    shouldBe(array[0], 42);
}
for (var i = 0; i < 1e5; ++i) {
    var array = [, 2, 3, 4];
    ensureArrayStorage(array);
    shouldBe(array[0], undefined);
    shouldBe(array[1], 2);
    testArrayStorageInBounds(array, 0, 42);
    testArrayStorageInBounds(array, 1, 40);
    shouldBe(array[0], 42);
    shouldBe(array[1], 40);
    shouldBe(array.length, 4);
    testArrayStorageInBounds(array, 4, 42);
    shouldBe(array[4], 42);
    shouldBe(array.length, 5);
}
for (var i = 0; i < 1e5; ++i) {
    var array = [, 2, 3, 4];
    ensureArrayStorage(array);
    shouldBe(array[6], undefined);
    testArrayStorageInBounds(array, 6, 42);
    shouldBe(array.length, 7);
    shouldBe(array[6], 42);
}
