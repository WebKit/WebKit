function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(array) {
    return array[0] * array[0];
}
noInline(test);

let jsarray = [42];
let f32array = new Float32Array([42]);
let f64array = new Float64Array([42]);
let i32array = new Int32Array([42]);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(test(jsarray), 42 * 42);
    shouldBe(test(f32array), 42 * 42);
    shouldBe(test(f64array), 42 * 42);
    shouldBe(test(i32array), 42 * 42);
}
