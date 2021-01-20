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

(function () {
    var array = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
    var array2 = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
    var array3 = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];

    array3[9] = 8;
    array3[8] = 10.2;

    for (var i = 0; i < 1e6; ++i)
        shouldBe(indexOf(array, 8), 8);

    array[9] = 8;
    array[8] = 10.2;

    for (var i = 0; i < 1e6; ++i)
        shouldBe(indexOf(array, 8), 9);

    for (var i = 0; i < 1e6; ++i)
        shouldBe(indexOf(array2, 8), 8);
}());
