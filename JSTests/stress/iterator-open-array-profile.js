//@ defaultRun
//@ runNoJIT

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function sumForOf(iterable) {
    var sum = 0;
    for (var value of iterable)
        sum += value;
    return sum;
}
noInline(sumForOf);

var u8 = new Uint8Array([1, 2, 3, 4]);
var f64 = new Float64Array([0.5, 1.5, 2.5]);
var i32 = new Int32Array(10);
for (var i = 0; i < i32.length; ++i)
    i32[i] = i;

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(sumForOf(u8), 10);
    shouldBe(sumForOf(f64), 4.5);
    shouldBe(sumForOf(i32), 45);
}

function countForOf(iterable) {
    var count = 0;
    for (var value of iterable)
        ++count;
    return count;
}
noInline(countForOf);

var custom = {
    [Symbol.iterator]() {
        var i = 0;
        return { next() { return i < 3 ? { done: false, value: i++ } : { done: true }; } };
    }
};

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(countForOf(u8), 4);
    shouldBe(countForOf(custom), 3);
    shouldBe(countForOf([1, 2, 3]), 3);
    shouldBe(countForOf('abc'), 3);
}
