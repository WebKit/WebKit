function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function indexOf(array, value)
{
    return array.indexOf(value);
}
noInline(indexOf);

var array = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
for (var i = 0; i < 1e6; ++i) {
    if (i === 1e6 - 1) {
        array[8] = "Hello";
        shouldBe(indexOf(array, 8), -1);
    } else
        shouldBe(indexOf(array, 8), 8);
}
