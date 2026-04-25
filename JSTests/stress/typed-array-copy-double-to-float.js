function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + " expected " + expected);
}

let array = [0.0, 0.1];
for (let i = 0; i < 1024; ++i)
    array[i] = 1.0 * i;

let f32 = new Float32Array(array);
for (let i = 0; i < 1024; ++i)
    shouldBe(f32[i], 1.0 * i);
