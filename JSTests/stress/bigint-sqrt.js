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

shouldThrow(() => BigInt.sqrt(), `TypeError: BigInt.sqrt requires the argument to be a positive BigInt`);
for (let value of [ false, true, -1, 0, 1, "-1", "0", "1", undefined, null, [ ], { } ]) {
    shouldThrow(() => BigInt.sqrt(value), `TypeError: BigInt.sqrt requires the argument to be a positive BigInt`);
    shouldThrow(() => BigInt.sqrt(value, 2n), `TypeError: BigInt.sqrt requires the argument to be a positive BigInt`);
    shouldBe(BigInt.sqrt(4n, value), 2n);
    shouldBe(BigInt.sqrt(15_241_578_750_190_521n, value), 123_456_789n);
}

for (let value of [ -1n, -2n, -4n, -8n, -9n, -15n, -16n, -24n, -25n, -15_241_578_750_190_521n, -15_241_578_780_673_678_515_622_620_750_190_521n ])
    shouldThrow(() => BigInt.sqrt(value), `RangeError: BigInt.sqrt requires the argument to be a positive BigInt`);

shouldBe(BigInt.sqrt(0n), 0n);
shouldBe(BigInt.sqrt(-0n), 0n);
shouldBe(BigInt.sqrt(1n), 1n);
shouldBe(BigInt.sqrt(2n), 1n);
shouldBe(BigInt.sqrt(4n), 2n);
shouldBe(BigInt.sqrt(8n), 2n);
shouldBe(BigInt.sqrt(9n), 3n);
shouldBe(BigInt.sqrt(15n), 3n);
shouldBe(BigInt.sqrt(16n), 4n);
shouldBe(BigInt.sqrt(24n), 4n);
shouldBe(BigInt.sqrt(25n), 5n);
shouldBe(BigInt.sqrt(15_241_578_750_190_521n), 123_456_789n);
shouldBe(BigInt.sqrt(15_241_578_780_673_678_515_622_620_750_190_521n), 123_456_789_123_456_789n);

// Heap values around 2^53 where a `Number`-based `Math.sqrt` would be off-by-one;
// these exercise the exact integer square-root path.
shouldBe(BigInt.sqrt(9_007_199_136_250_224n), 94_906_264n);
shouldBe(BigInt.sqrt(9_007_199_136_250_225n), 94_906_265n);
shouldBe(BigInt.sqrt(9_007_199_136_250_226n), 94_906_265n);
shouldBe(BigInt.sqrt(9_007_199_254_740_991n), 94_906_265n);
shouldBe(BigInt.sqrt(9_007_199_254_740_992n), 94_906_265n);
shouldBe(BigInt.sqrt(15_241_578_780_560_891_109_129n), 123_456_789_123n);
