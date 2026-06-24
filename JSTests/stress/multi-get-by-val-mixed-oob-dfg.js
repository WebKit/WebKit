//@ runDefault("--useFTLJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}
noInline(shouldBe);

function test(array, index) {
    return array[index];
}
noInline(test);

let jsInt32 = [42];
let jsDouble = [5.5];
let jsContiguous = ["x"];
let f32array = new Float32Array([42]);
let f64array = new Float64Array([5.5]);
let i32array = new Int32Array([42]);
let u8array = new Uint8Array([42]);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(test(jsInt32, 0), 42);
    shouldBe(test(jsDouble, 0), 5.5);
    shouldBe(test(jsContiguous, 0), "x");
    shouldBe(test(f32array, 0), 42);
    shouldBe(test(f64array, 0), 5.5);
    shouldBe(test(i32array, 0), 42);
    shouldBe(test(u8array, 0), 42);

    shouldBe(test(jsInt32, 1), undefined);
    shouldBe(test(jsDouble, 1), undefined);
    shouldBe(test(jsContiguous, 1), undefined);
    shouldBe(test(f32array, 1), undefined);
    shouldBe(test(f64array, 1), undefined);
    shouldBe(test(i32array, 1), undefined);
    shouldBe(test(u8array, 1), undefined);

    shouldBe(test(jsInt32, 100), undefined);
    shouldBe(test(jsDouble, 100), undefined);
    shouldBe(test(f32array, 100), undefined);
    shouldBe(test(i32array, 100), undefined);
}
