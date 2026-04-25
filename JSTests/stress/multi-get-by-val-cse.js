function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(array) {
    var r = array[0] * array[0];
    array[0] = 33;
    return r * array[0];
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(test([5.5]), 5.5 * 5.5 * 33);
    shouldBe(test(new Float32Array([5.5])), 5.5 * 5.5 * 33);
    shouldBe(test(new Float64Array([5.5])), 5.5 * 5.5 * 33);
}
