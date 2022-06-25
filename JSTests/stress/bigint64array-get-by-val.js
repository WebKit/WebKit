function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array1 = new BigInt64Array([-1n, -2n, -3n]);
var array2 = new BigUint64Array([1n, 2n, 3n]);

function test1(array) {
    var result = 0n;
    for (var i = 0; i < array.length; ++i)
        result += array[i];
    return result;
}
noInline(test1);

function test2(array) {
    var result = 0n;
    for (var i = 0; i < array.length; ++i)
        result += array[i];
    return result;
}
noInline(test2);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(test1(array1), -6n);
    shouldBe(test2(array2), 6n);
}
