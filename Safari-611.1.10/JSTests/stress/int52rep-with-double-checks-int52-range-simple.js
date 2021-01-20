function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var doubleArray = [3, 1, 2, 3, 4, 5.5];
doubleArray[0] = 3; // Break CoW.

function test(array)
{
    return fiatInt52(array[0]);
}
noInline(test);

for (var i = 0; i < 1e7; ++i)
    shouldBe(test(doubleArray), 3);
doubleArray[0] = 0x7ffffffffffff;
shouldBe(test(doubleArray), 0x7ffffffffffff);
doubleArray[0] = 0x8000000000000;
shouldBe(test(doubleArray), 0x8000000000000);
doubleArray[0] = -0x8000000000000;
shouldBe(test(doubleArray), -0x8000000000000);
doubleArray[0] = -0x8000000000001;
shouldBe(test(doubleArray), -0x8000000000001);
doubleArray[0] = 1.3;
shouldBe(test(doubleArray), 1.3);
doubleArray[0] = Number.NaN;
shouldBe(Number.isNaN(test(doubleArray)), true);
doubleArray[0] = Number.POSITIVE_INFINITY;
shouldBe(test(doubleArray), Number.POSITIVE_INFINITY);
doubleArray[0] = Number.NEGATIVE_INFINITY;
shouldBe(test(doubleArray), Number.NEGATIVE_INFINITY);

doubleArray[0] = 3;
shouldBe(test(doubleArray), 3);
