function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    function indexOfInt32(array, value, index)
    {
        return array.indexOf(value, index);
    }
    noInline(indexOfInt32);

    function indexOfDouble(array, value, index)
    {
        return array.indexOf(value, index);
    }
    noInline(indexOfDouble);

    function indexOfString(array, value, index)
    {
        return array.indexOf(value, index);
    }
    noInline(indexOfString);

    function indexOfObject(array, value, index)
    {
        return array.indexOf(value, index);
    }
    noInline(indexOfObject);

    var key = {};
    var int32Array = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12];
    var doubleArray = [0, 1, 2, 3, 4.2, 5, 6, 7, 8, 9, 10.5, 11, 12];
    var stringArray = [ "cocoa", "cappuccino", "matcha", "rize", "kilimanjaro" ];
    var objectArray = [ {}, {}, {}, {}, {}, key, {}, {}, {}, null, undefined ];

    for (var i = 0; i < 1e5; ++i) {
        shouldBe(indexOfInt32(int32Array, null, 0), -1);
        shouldBe(indexOfInt32(int32Array, undefined, 0), -1);
        shouldBe(indexOfDouble(doubleArray, null, 0), -1);
        shouldBe(indexOfDouble(doubleArray, undefined, 0), -1);
        shouldBe(indexOfDouble(doubleArray, null, 0), -1);
        shouldBe(indexOfDouble(doubleArray, undefined, 0), -1);
        shouldBe(indexOfString(stringArray, null, 0), -1);
        shouldBe(indexOfString(stringArray, undefined, 0), -1);
        shouldBe(indexOfObject(objectArray, null, 0), 9);
        shouldBe(indexOfObject(objectArray, undefined, 0), 10);
    }
}());
