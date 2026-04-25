function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(array, index) {
    return array[index];
}
noInline(test);

let jsarray = [5.5];
let f32array = new Float32Array([5.5]);
let f64array = new Float64Array([5.5]);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(test(jsarray, 0), 5.5);
    shouldBe(test(f32array, 0), 5.5);
    shouldBe(test(f64array, 0), 5.5);
    shouldBe(test(jsarray, 1), undefined);
    shouldBe(test(f32array, 1), undefined);
    shouldBe(test(f64array, 1), undefined);
}
