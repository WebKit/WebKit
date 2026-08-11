function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = new Float64Array(1024);
for (var i = 0; i < 1024; ++i) {
    array[i] = 42;
}

shouldBe(array.includes(NaN), false);
array[1024 - 5] = NaN;
shouldBe(array.includes(NaN), true);


shouldBe(array.includes(Infinity), false);
shouldBe(array.includes(-Infinity), false);
array[1024 - 4] = Infinity;
shouldBe(array.includes(Infinity), true);
shouldBe(array.includes(-Infinity), false);
array[1024 - 3] = -Infinity;
shouldBe(array.includes(Infinity), true);
shouldBe(array.includes(-Infinity), true);

shouldBe(array.includes(-0), false);
shouldBe(array.includes(0), false);
array[1024 - 2] = -0;
shouldBe(array.includes(-0), true);
shouldBe(array.includes(0), true);
array[1024 - 1] = 0;
shouldBe(array.includes(-0), true);
shouldBe(array.includes(0), true);
