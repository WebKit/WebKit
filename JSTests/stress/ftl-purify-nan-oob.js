function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array, index) {
    return array[index];
}
noInline(test);

var bytes = new BigUint64Array([0xfffe000000000000n]);
var array = new Float64Array(bytes.buffer);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(isPureNaN(test(array, 0)), true);
    test(array, 1)
}
