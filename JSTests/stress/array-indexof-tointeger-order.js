function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
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

    var int32Array = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12];
    var doubleArray = [0, 1, 2, 3, 4.2, 5, 6, 7.33, 8, 9, 10.5];
    var stringArray = ["cocoa", "cappuccino", "matcha", "rize"];

    for (var i = 0; i < 1e5; ++i) {
        assert(indexOfInt32(int32Array, 3, 2) === 3);
        assert(indexOfDouble(doubleArray, 3, 1) === 3);
        assert(indexOfString(stringArray, "cocoa", 0) === 0);
    }

    int32Array.length = 0;
    doubleArray.splice(0);
    while (stringArray.length) stringArray.pop();

    assert($vm.indexingMode(int32Array) !== "ArrayWithUndecided");
    assert($vm.indexingMode(doubleArray) !== "ArrayWithUndecided");
    assert($vm.indexingMode(stringArray) !== "ArrayWithUndecided");

    var fromIndex = {
        get valueOf() {
            throw new Error("fromIndex.valueOf");
        },
    };

    for (var i = 0; i < 1e5; ++i) {
        assert(indexOfInt32(int32Array, 3, fromIndex) === -1);
        assert(indexOfDouble(doubleArray, 3, fromIndex) === -1);
        assert(indexOfString(stringArray, "cocoa", fromIndex) === -1);
    }
}());
