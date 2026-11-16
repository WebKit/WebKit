function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(array) {
    return array[0] * array[0];
}
noInline(test);

let jsarray = [5.5];
let f32array = new Float32Array([5.5]);
let f64array = new Float64Array([5.5]);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(test(jsarray), 5.5 * 5.5);
    shouldBe(test(f32array), 5.5 * 5.5);
    shouldBe(test(f64array), 5.5 * 5.5);
}
