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

shouldThrow(() => BigInt.min(), `TypeError: BigInt.min requires every argument to be a BigInt`);
for (let value of [ false, true, -1, 0, 1, "-1", "0", "1", undefined, null, [ ], { } ]) {
    shouldThrow(() => BigInt.min(value), `TypeError: BigInt.min requires every argument to be a BigInt`);
    shouldThrow(() => BigInt.min(1n, value), `TypeError: BigInt.min requires every argument to be a BigInt`);
    shouldThrow(() => BigInt.min(value, 2n), `TypeError: BigInt.min requires every argument to be a BigInt`);
    shouldThrow(() => BigInt.min(1n, value, 3n), `TypeError: BigInt.min requires every argument to be a BigInt`);
}

shouldBe(BigInt.min(0n), 0n);
shouldBe(BigInt.min(-0n), 0n);
shouldBe(BigInt.min(1n), 1n);
shouldBe(BigInt.min(-1n), -1n);
shouldBe(BigInt.min(123_456_789n), 123_456_789n);
shouldBe(BigInt.min(-123_456_789n), -123_456_789n);
shouldBe(BigInt.min(123_456_789_123_456_789n), 123_456_789_123_456_789n);
shouldBe(BigInt.min(-123_456_789_123_456_789n), -123_456_789_123_456_789n);

shouldBe(BigInt.min(0n, 0n), 0n);
shouldBe(BigInt.min(-0n, -0n), 0n);
shouldBe(BigInt.min(1n, 1n), 1n);
shouldBe(BigInt.min(-1n, -1n), -1n);
shouldBe(BigInt.min(123_456_789n, 123_456_789n), 123_456_789n);
shouldBe(BigInt.min(-123_456_789n, -123_456_789n), -123_456_789n);
shouldBe(BigInt.min(123_456_789_123_456_789n, 123_456_789_123_456_789n), 123_456_789_123_456_789n);
shouldBe(BigInt.min(-123_456_789_123_456_789n, -123_456_789_123_456_789n), -123_456_789_123_456_789n);

shouldBe(BigInt.min(0n, -0n), 0n);
shouldBe(BigInt.min(-0n, 0n), 0n);
shouldBe(BigInt.min(1n, -1n), -1n);
shouldBe(BigInt.min(-1n, 1n), -1n);
shouldBe(BigInt.min(123_456_789n, -123_456_789n), -123_456_789n);
shouldBe(BigInt.min(-123_456_789n, 123_456_789n), -123_456_789n);
shouldBe(BigInt.min(123_456_789_123_456_789n, -123_456_789_123_456_789n), -123_456_789_123_456_789n);
shouldBe(BigInt.min(-123_456_789_123_456_789n, 123_456_789_123_456_789n), -123_456_789_123_456_789n);

shouldBe(BigInt.min(-123_456_789_123_456_789n, -123_456_789n, -1n, -0n, 0n, 1n, 123_456_789n, 123_456_789_123_456_789n), -123_456_789_123_456_789n);
shouldBe(BigInt.min(123_456_789_123_456_789n, 123_456_789n, 1n, 0n, -0n, -1n, -123_456_789n, -123_456_789_123_456_789n), -123_456_789_123_456_789n);
shouldBe(BigInt.min(123_456_789_123_456_789n, -123_456_789_123_456_789n, 123_456_789n, -123_456_789n, 1n, -1n, 0n, -0n), -123_456_789_123_456_789n);
shouldBe(BigInt.min(0n, -0n, 1n, -1n, 123_456_789n, -123_456_789n, 123_456_789_123_456_789n, -123_456_789_123_456_789n), -123_456_789_123_456_789n);

