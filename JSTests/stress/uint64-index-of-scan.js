function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    var array = new BigUint64Array(300);
    for (var i = 0; i < 300; ++i)
        array[i] = BigInt(i);

    for (var i = 0; i < 300; ++i) {
        for (var j = 0; j < 300; ++j)
            shouldBe(array.indexOf(BigInt(i), j), i < j ? -1 : i);
    }
}());
