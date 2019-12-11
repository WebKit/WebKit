function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [];

for (var i = 0; i < 100; ++i)
    array.push(1024 * 1024 * 1024 * 1024 + i);
for (var i = 0; i < 100; ++i)
    array.push(-(1024 * 1024 * 1024 * 1024 + i));

array.push(2251799813685248);
array.push(0.5);

function test(array, index, value)
{
    return array[index] + fiatInt52(value);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    for (var index = 0; index < 100; ++index)
        shouldBe(test(array, index, 20), 1024 * 1024 * 1024 * 1024 + index + 20);
    for (var index = 0; index < 100; ++index)
        shouldBe(test(array, index + 100, 20), -(1024 * 1024 * 1024 * 1024 + index) + 20);
}

// Int52Overflow.
shouldBe(test(array, 200, 200), 2251799813685448);

// Not AnyIntAsDouble, Int52Overflow.
shouldBe(test(array, 201, 200), 200.5);

// Recompile the code as ArithAdd(Double, Double).
for (var i = 0; i < 1e4; ++i)
    shouldBe(test(array, 201, 200), 200.5);

shouldBe(test(array, 200, 200), 2251799813685448);
shouldBe(test(array, 201, 200), 200.5);


