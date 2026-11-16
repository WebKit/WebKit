function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function () {
    var array = new Uint8Array(256);
    for (var i = 0; i < 256; ++i)
        array[i] = i;

    for (var i = 0; i < 256; ++i) {
        for (var j = 0; j < 256; ++j)
            shouldBe(array.indexOf(i, j), i < j ? -1 : i);
    }
}());
