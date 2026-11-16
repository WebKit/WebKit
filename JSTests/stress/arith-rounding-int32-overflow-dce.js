//@ requireOptions("--useConcurrentJIT=0", "--thresholdForFTLOptimizeAfterWarmUp=1000")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("FAIL: got " + actual + ", expected " + expected);
}

function floorOverflow(k) {
    let y = Math.floor(k);
    return (y | 0) === y;
}
noInline(floorOverflow);

function ceilOverflow(k) {
    let y = Math.ceil(k);
    return (y | 0) === y;
}
noInline(ceilOverflow);

function roundOverflow(k) {
    let y = Math.round(k);
    return (y | 0) === y;
}
noInline(roundOverflow);

function truncOverflow(k) {
    let y = Math.trunc(k);
    return (y | 0) === y;
}
noInline(truncOverflow);

function floorNegZero(k) {
    let y = Math.floor(k);
    return Object.is(y | 0, y);
}
noInline(floorNegZero);

for (let i = 0; i < 1e6; ++i) {
    floorOverflow(1.5);
    ceilOverflow(1.2);
    roundOverflow(1.2);
    truncOverflow(1.5);
    floorNegZero(1.5);
}

shouldBe(floorOverflow(2147483648.5), false);
shouldBe(ceilOverflow(2147483647.2), false);
shouldBe(roundOverflow(2147483647.6), false);
shouldBe(truncOverflow(2147483648.5), false);
shouldBe(floorNegZero(-0), false);
shouldBe(floorOverflow(1.5), true);
