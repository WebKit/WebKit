function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array1 = new BigInt64Array([-1n, -2n, -3n]);
var array2 = new BigUint64Array([1n, 2n, 3n]);

function test11(array, value) {
    for (var i = 0; i < array.length; ++i)
        array[i] = value;
}
noInline(test11);

function test12(array) {
    var result = 0n;
    for (var i = 0; i < array.length; ++i)
        result += array[i];
    return result;
}
noInline(test12);

function test21(array, value) {
    for (var i = 0; i < array.length; ++i)
        array[i] = value;
}
noInline(test21);

function test22(array) {
    var result = 0n;
    for (var i = 0; i < array.length; ++i)
        result += array[i];
    return result;
}
noInline(test22);

for (var i = 0; i < 1e5; ++i) {
    test11(array1, -1n)
    shouldBe(test12(array1), -3n);
    test11(array1, -2n);
    shouldBe(test12(array1), -6n);
    test21(array2, 1n);
    shouldBe(test22(array2), 3n);
    test21(array2, 2n);
    shouldBe(test22(array2), 6n);
}
