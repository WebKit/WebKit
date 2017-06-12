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

    function indexOfValue(array, value, index)
    {
        return array.indexOf(value, index);
    }
    noInline(indexOfValue);

    var key = {};
    var int32Array = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12];
    var doubleArray = [0, 1, 2, 3, 4.2, 5, 6, 7, 8, 9, 10.5, 11, 12];
    var stringArray = [ "cocoa", "cappuccino", "matcha", "rize", "kilimanjaro" ];
    var objectArray = [ {}, {}, {}, {}, {}, key, {}, {}, {} ];
    var valueArray = [ {}, {}, {}, {}, {}, null, {}, {}, {} ];

    for (var i = 0; i < 1e5; ++i) {
        shouldBe(indexOfInt32(int32Array, 3, 0), 3);
        shouldBe(indexOfInt32(int32Array, 3, 8), -1);
        shouldBe(indexOfDouble(doubleArray, 3, 0), 3);
        shouldBe(indexOfDouble(doubleArray, 3, 20), -1);
        shouldBe(indexOfDouble(doubleArray, 4.2, 8), -1);
        shouldBe(indexOfDouble(doubleArray, 4.2, 0), 4);
        shouldBe(indexOfDouble(doubleArray, 4.2, 20), -1);
        shouldBe(indexOfString(stringArray, "cocoa", 0), 0);
        shouldBe(indexOfString(stringArray, "cocoa", 4), -1);
        shouldBe(indexOfString(stringArray, "cocoa", 20), -1);
        shouldBe(indexOfObject(objectArray, key, 0), 5);
        shouldBe(indexOfObject(objectArray, key, 6), -1);
        shouldBe(indexOfObject(objectArray, key, 20), -1);
        shouldBe(indexOfValue(valueArray, null, 0), 5);
        shouldBe(indexOfValue(valueArray, null, 6), -1);
        shouldBe(indexOfValue(valueArray, null, 20), -1);
    }
}());
