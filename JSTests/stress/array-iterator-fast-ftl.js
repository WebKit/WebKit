// FTL tier coverage for FastArrayValues/Keys/Entries — force tier-up to FTL and verify correctness.
//@ runFTLNoCJIT
//@ runDefault("--useConcurrentJIT=1", "--thresholdForFTLOptimizeAfterWarmUp=10", "--thresholdForFTLOptimizeSoon=10", "--thresholdForOptimizeAfterWarmUp=10")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function values(array) {
    var sum = 0;
    for (var v of array.values())
        sum += v;
    return sum;
}
noInline(values);

function keys(array) {
    var sum = 0;
    for (var k of array.keys())
        sum += k;
    return sum;
}
noInline(keys);

function entries(array) {
    var sum = 0;
    for (var [k, v] of array.entries())
        sum += k * 1000 + v;
    return sum;
}
noInline(entries);

var array = [];
for (var i = 0; i < 100; ++i)
    array.push(i + 1);

var expectedValues = 0, expectedKeys = 0, expectedEntries = 0;
for (var i = 0; i < 100; ++i) {
    expectedValues += i + 1;
    expectedKeys += i;
    expectedEntries += i * 1000 + (i + 1);
}

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(array), expectedValues);
    shouldBe(keys(array), expectedKeys);
    shouldBe(entries(array), expectedEntries);
}
