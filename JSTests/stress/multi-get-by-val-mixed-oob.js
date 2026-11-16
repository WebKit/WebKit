function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(array, index) {
    return array[index];
}
noInline(test);

let jsarray = [42];
let f32array = new Float32Array([42]);
let f64array = new Float64Array([42]);
let i32array = new Int32Array([42]);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(test(jsarray, 0), 42);
    shouldBe(test(f32array, 0), 42);
    shouldBe(test(f64array, 0), 42);
    shouldBe(test(i32array, 0), 42);
    shouldBe(test(jsarray, 1), undefined);
    shouldBe(test(f32array, 1), undefined);
    shouldBe(test(f64array, 1), undefined);
    shouldBe(test(i32array, 1), undefined);
}
