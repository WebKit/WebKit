//@ runDefault("--useFTLJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

function test(array, index, value) {
    array[index] = value;
    return array;
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    let int32 = [1, 2, 3, 4];
    shouldBeArray(test(int32, 0, 42), [42, 2, 3, 4]);
    shouldBeArray(test(int32, 3, 99), [42, 2, 3, 99]);

    let double = [1.5, 2.5, 3.5];
    shouldBeArray(test(double, 1, 7), [1.5, 7, 3.5]);

    let contiguous = ["a", "b", "c"];
    shouldBeArray(test(contiguous, 0, 11), [11, "b", "c"]);

    let i32a = new Int32Array([0, 0, 0]);
    test(i32a, 0, 42);
    test(i32a, 2, -7);
    shouldBeArray(i32a, [42, 0, -7]);

    let u8a = new Uint8Array([0, 0, 0]);
    test(u8a, 0, 200);
    test(u8a, 1, 300); // wraps mod 256
    shouldBeArray(u8a, [200, 44, 0]);

    let u8c = new Uint8ClampedArray([0, 0, 0]);
    test(u8c, 0, 300); // clamps to 255
    test(u8c, 1, -5);  // clamps to 0
    test(u8c, 2, 100);
    shouldBeArray(u8c, [255, 0, 100]);

    let f32a = new Float32Array([0, 0, 0]);
    test(f32a, 0, 7);
    shouldBeArray(f32a, [7, 0, 0]);

    let f64a = new Float64Array([0, 0, 0]);
    test(f64a, 0, 13);
    shouldBeArray(f64a, [13, 0, 0]);
}
