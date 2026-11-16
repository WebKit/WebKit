//@ requireOptions("--useConcurrentJIT=0", "--thresholdForFTLOptimizeAfterWarmUp=1000")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("FAIL: got " + actual + ", expected " + expected);
}

function mulOverflow(k) {
    let y = k;
    y = y * y;
    return (y | 0) === y;
}
noInline(mulOverflow);

function mulNegativeZero(a, b) {
    let y = a;
    y = y * b;
    return Object.is(y | 0, y);
}
noInline(mulNegativeZero);

function divNonInteger(a, b) {
    let y = a;
    y = y / b;
    return (y | 0) === y;
}
noInline(divNonInteger);

function modNegativeZero(a, b) {
    let y = a;
    y = y % b;
    return Object.is(y | 0, y);
}
noInline(modNegativeZero);

function negateOverflow(k) {
    let y = k;
    y = -y;
    return (y | 0) === y;
}
noInline(negateOverflow);

function negateNegativeZero(k) {
    let y = k;
    y = -y;
    return Object.is(y | 0, y);
}
noInline(negateNegativeZero);

function absOverflow(k) {
    let y = k;
    y = Math.abs(y);
    return (y | 0) === y;
}
noInline(absOverflow);

for (let i = 0; i < testLoopCount; ++i) {
    mulOverflow(3);
    mulNegativeZero(3, 2);
    divNonInteger(6, 3);
    modNegativeZero(7, 3);
    negateOverflow(3);
    negateNegativeZero(3);
    absOverflow(-3);
}

shouldBe(mulOverflow(65536), false);
shouldBe(mulNegativeZero(0, -1), false);
shouldBe(divNonInteger(7, 2), false);
shouldBe(modNegativeZero(-3, 3), false);
shouldBe(negateOverflow(-2147483648), false);
shouldBe(negateNegativeZero(0), false);
shouldBe(absOverflow(-2147483648), false);
