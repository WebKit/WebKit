//@ skip if not $jitTests
//@ runDefault("--useConcurrentJIT=0", "--useFTLJIT=0", "--thresholdForJITAfterWarmUp=2", "--thresholdForOptimizeAfterWarmUp=20")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function storeIntThenBoolean(a, b) {
    const result = { type: "string", value: 42 };
    result[0] = a;
    result[1] = b;
    return result;
}
noInline(storeIntThenBoolean);

function storeIntThenDouble(a, b) {
    const result = { type: "string", value: 42 };
    result[0] = a;
    result[1] = b;
    return result;
}
noInline(storeIntThenDouble);

function storeDoubleThenString(a, b) {
    const result = { type: "string", value: 42 };
    result[0] = a;
    result[1] = b;
    return result;
}
noInline(storeDoubleThenString);

for (let i = 0; i < testLoopCount; ++i) {
    const first = storeIntThenBoolean(i, true);
    shouldBe(first[0], i);
    shouldBe(first[1], true);
    const second = storeIntThenDouble(i, i + 0.5);
    shouldBe(second[0], i);
    shouldBe(second[1], i + 0.5);
    const third = storeDoubleThenString(i + 0.5, "s");
    shouldBe(third[0], i + 0.5);
    shouldBe(third[1], "s");
}
shouldBe(numberOfDFGCompiles(storeIntThenBoolean) <= 1, true);
shouldBe(numberOfDFGCompiles(storeIntThenDouble) <= 1, true);
shouldBe(numberOfDFGCompiles(storeDoubleThenString) <= 1, true);
