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
    var value = 3.3;
    if (i === 1e5 - 1)
        value = NaN;
    shouldBe(test(array, 1.1, 2.2, value), 3);
    shouldBe(array[0], 1.1);
    shouldBe(array[1], 2.2);
    if (i === 1e5 - 1)
        shouldBe(Number.isNaN(array[2]), true);
    else
        shouldBe(array[2], 3.3);
}
