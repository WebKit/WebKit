function test1(array) {
    return array.byteLength;
}
noInline(test1);

function test2(array) {
    return array.byteLength;
}
noInline(test2);

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array1 = new BigInt64Array(4);
var array2 = new BigUint64Array(4);
for (var i = 0; i < 1e6; ++i) {
    shouldBe(test1(array1), 32);
    shouldBe(test2(array2), 32);
}
