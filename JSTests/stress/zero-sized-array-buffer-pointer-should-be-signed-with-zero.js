function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var typedArray = new Int8Array();
shouldBe(typedArray.length, 0);
var subarray = typedArray.subarray(0, 0);
shouldBe(subarray.length, 0);
