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

    var array = [];
    var object = {};
    for (var i = 0; i < 100; ++i)
        array.push({});
    array.push(object);

    for (var i = 0; i < 1e5; ++i)
        shouldBe(indexOf(array, object), 100);
}());

(function () {
    function indexOf(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOf);

    var array = [];
    var object = {};
    for (var i = 0; i < 100; ++i) {
        array.push(42);
        array.push({});
        array.push(String(i));
    }
    array.push(object);

    for (var i = 0; i < 1e5; ++i)
        shouldBe(indexOf(array, object), 100 * 3);
}());
