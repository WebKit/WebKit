function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = new Float32Array(1024);
for (var i = 0; i < 1024; ++i) {
    array[i] = 42;
}
array[1024 - 5] = NaN;
array[1024 - 4] = Infinity;
array[1024 - 3] = -Infinity;
array[1024 - 2] = -0;
array[1024 - 1] = 0;

shouldBe(array.indexOf(NaN), -1);
shouldBe(array.indexOf(Infinity), 1020);
shouldBe(array.indexOf(-Infinity), 1021);
// Negative / Positive zeros are evaluated as the same.
shouldBe(array.indexOf(0), 1022);
shouldBe(array.indexOf(-0), 1022);
