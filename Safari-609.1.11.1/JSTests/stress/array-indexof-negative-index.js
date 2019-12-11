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
        shouldBe(indexOfInt32(int32Array, 3, -2), -1);
        shouldBe(indexOfInt32(int32Array, 3, -10), 3);
        shouldBe(indexOfInt32(int32Array, 3, -20), 3);
        shouldBe(indexOfDouble(doubleArray, 3, -1), -1);
        shouldBe(indexOfDouble(doubleArray, 3, -10), 3);
        shouldBe(indexOfDouble(doubleArray, 3, -20), 3);
        shouldBe(indexOfDouble(doubleArray, 4.2, -8), -1);
        shouldBe(indexOfDouble(doubleArray, 4.2, -1), -1);
        shouldBe(indexOfDouble(doubleArray, 4.2, -10), 4);
        shouldBe(indexOfDouble(doubleArray, 4.2, -20), 4);
        shouldBe(indexOfString(stringArray, "cocoa", -1), -1);
        shouldBe(indexOfString(stringArray, "cocoa", -4), -1);
        shouldBe(indexOfString(stringArray, "cocoa", -5), 0);
        shouldBe(indexOfString(stringArray, "cocoa", -20), 0);
        shouldBe(indexOfObject(objectArray, key, -1), -1);
        shouldBe(indexOfObject(objectArray, key, -2), -1);
        shouldBe(indexOfObject(objectArray, key, -3), -1);
        shouldBe(indexOfObject(objectArray, key, -4), 5);
        shouldBe(indexOfObject(objectArray, key, -6), 5);
        shouldBe(indexOfObject(objectArray, key, -20), 5);
        shouldBe(indexOfValue(valueArray, null, -1), -1);
        shouldBe(indexOfValue(valueArray, null, -3), -1);
        shouldBe(indexOfValue(valueArray, null, -4), 5);
        shouldBe(indexOfValue(valueArray, null, -6), 5);
        shouldBe(indexOfValue(valueArray, null, -20), 5);
    }
}());
