//@ requireOptions("--useBigIntMathMethods=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

shouldThrow(() => BigInt.abs(), `TypeError: BigInt.abs requires the argument to be a BigInt`);
for (let value of [ false, true, -1, 0, 1, "-1", "0", "1", undefined, null, [ ], { } ]) {
    shouldThrow(() => BigInt.abs(value), `TypeError: BigInt.abs requires the argument to be a BigInt`);
    shouldBe(BigInt.abs(1n, value), 1n);
    shouldBe(BigInt.abs(-123_456_789_123_456_789n, value), 123_456_789_123_456_789n);
}

shouldBe(BigInt.abs(0n), 0n);
shouldBe(BigInt.abs(-0n), 0n);

shouldBe(BigInt.abs(1n), 1n);
shouldBe(BigInt.abs(-1n), 1n);

shouldBe(BigInt.abs(123_456_789n), 123_456_789n);
shouldBe(BigInt.abs(-123_456_789n), 123_456_789n);

shouldBe(BigInt.abs(123_456_789_123_456_789n), 123_456_789_123_456_789n);
shouldBe(BigInt.abs(-123_456_789_123_456_789n), 123_456_789_123_456_789n);
