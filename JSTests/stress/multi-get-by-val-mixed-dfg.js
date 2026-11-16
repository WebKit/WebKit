//@ runDefault("--useFTLJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}
noInline(shouldBe);

function test(array) {
    return array[0] * array[0];
}
noInline(test);

let jsInt32 = [42];
let jsDouble = [5.5];
let jsContiguous = ["x", 7];
let f32array = new Float32Array([42]);
let f64array = new Float64Array([5.5]);
let i32array = new Int32Array([42]);
let u8array = new Uint8Array([42]);
let u8clamped = new Uint8ClampedArray([42]);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(test(jsInt32), 42 * 42);
    shouldBe(test(jsDouble), 5.5 * 5.5);
    shouldBe(Number.isNaN(test(jsContiguous)), true);
    shouldBe(test(f32array), 42 * 42);
    shouldBe(test(f64array), 5.5 * 5.5);
    shouldBe(test(i32array), 42 * 42);
    shouldBe(test(u8array), 42 * 42);
    shouldBe(test(u8clamped), 42 * 42);
}
