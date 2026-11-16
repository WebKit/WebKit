function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let array = [0, 1, 2, 3, 4, 5];
    array.deletedCount = 42;

    array.splice(0, 3);
    shouldBe(array.length, 3);
    shouldBe(array[0], 3);
    shouldBe(array[1], 4);
    shouldBe(array[2], 5);
}
{
    let object = null;
    let array = [0, 1, 2, 3, 4, 5];
    array.constructor = function () { object = this; };
    array.constructor[Symbol.species] = array.constructor;
    array.deletedCount = 42;

    let result = array.splice(0, 3);
    shouldBe(array.length, 3);
    shouldBe(array[0], 3);
    shouldBe(array[1], 4);
    shouldBe(array[2], 5);

    shouldBe(result, object);
}
