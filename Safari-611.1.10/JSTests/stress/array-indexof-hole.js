function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    function indexOf(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOf);

    var array = new Array(100);
    array.push(10);

    for (var i = 0; i < 1e5; ++i)
        shouldBe(indexOf(array, 0), -1);
}());

(function () {
    function indexOf(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOf);

    var array = new Array(100);
    array.push(25.5);

    for (var i = 0; i < 1e5; ++i)
        shouldBe(indexOf(array, 0), -1);
}());
