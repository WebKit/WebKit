function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = new BigInt64Array([3n, 0n, 1n, 2n]);
array.sort(function (a, b) {
    shouldBe(typeof a, "bigint");
    shouldBe(typeof b, "bigint");
});
