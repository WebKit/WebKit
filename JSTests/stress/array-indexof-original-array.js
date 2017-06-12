function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    function indexOfInt32(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOfInt32);
    var int32Array = [0, 1, 2, 3, 4, , 6, 7, 8, 9, 10, 11, 12];

    var value = -1;
    for (var i = 0; i < 1e5; ++i) {
        shouldBe(indexOfInt32(int32Array, 5), value);
        shouldBe(indexOfInt32(int32Array, 6), 6);
        if (i === 1e4) {
            int32Array.hello = 42;
        }
    }
}());


(function () {
    function indexOfInt32(array, value)
    {
        return array.indexOf(value);
    }
    noInline(indexOfInt32);
    var int32Array = [0, 1, 2, 3, 4, , 6, 7, 8, 9, 10, 11, 12];

    var value = -1;
    for (var i = 0; i < 1e5; ++i) {
        shouldBe(indexOfInt32(int32Array, 5), value);
        shouldBe(indexOfInt32(int32Array, 6), 6);
        if (i === 1e4) {
            value = 5;
            int32Array.__proto__ = {
                __proto__: int32Array.__proto__,
                5: 5
            };
        }
    }
}());
