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

    for (var i = 0; i < 1e4; ++i) {
        shouldBe(indexOf(array, undefined), -1);
        shouldBe(indexOf(array, null), -1);
    }
}());

(function () {
    function indexOf(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOf);

    var array = new Array(100);
    array.push({});

    for (var i = 0; i < 1e4; ++i) {
        shouldBe(indexOf(array, undefined), -1);
        shouldBe(indexOf(array, null), -1);
    }
}());


