//@ requireOptions("--useConcurrentJIT=0", "--thresholdForFTLOptimizeAfterWarmUp=1000")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("FAIL: got " + actual + ", expected " + expected);
}

function inc(k) {
    let y = k;
    ++y;
    return (y | 0) === y;
}
noInline(inc);

function dec(k) {
    let y = k;
    --y;
    return (y | 0) === y;
}
noInline(dec);

for (let i = 0; i < 1e6; ++i) {
    inc(1);
    dec(1);
}

shouldBe(inc(2147483647), false);
shouldBe(dec(-2147483648), false);
