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

shouldThrow(() => BigInt.cbrt(), `TypeError: BigInt.cbrt requires the argument to be a BigInt`);
for (let value of [ false, true, -1, 0, 1, "-1", "0", "1", undefined, null, [ ], { } ]) {
    shouldThrow(() => BigInt.cbrt(value), `TypeError: BigInt.cbrt requires the argument to be a BigInt`);
    shouldThrow(() => BigInt.cbrt(value, 2n), `TypeError: BigInt.cbrt requires the argument to be a BigInt`);
    shouldBe(BigInt.cbrt(8n, value), 2n);
    shouldBe(BigInt.cbrt(-1_881_676_371_789_154_860_897_069n, value), -123_456_789n);
}

shouldBe(BigInt.cbrt(0n), 0n);
shouldBe(BigInt.cbrt(-0n), 0n);
shouldBe(BigInt.cbrt(1n), 1n);
shouldBe(BigInt.cbrt(-1n), -1n);
shouldBe(BigInt.cbrt(2n), 1n);
shouldBe(BigInt.cbrt(-2n), -1n);
shouldBe(BigInt.cbrt(7n), 1n);
shouldBe(BigInt.cbrt(-7n), -1n);
shouldBe(BigInt.cbrt(8n), 2n);
shouldBe(BigInt.cbrt(-8n), -2n);
shouldBe(BigInt.cbrt(26n), 2n);
shouldBe(BigInt.cbrt(-26n), -2n);
shouldBe(BigInt.cbrt(27n), 3n);
shouldBe(BigInt.cbrt(-27n), -3n);
shouldBe(BigInt.cbrt(63n), 3n);
shouldBe(BigInt.cbrt(-63n), -3n);
shouldBe(BigInt.cbrt(64n), 4n);
shouldBe(BigInt.cbrt(-64n), -4n);
shouldBe(BigInt.cbrt(124n), 4n);
shouldBe(BigInt.cbrt(-124n), -4n);
shouldBe(BigInt.cbrt(125n), 5n);
shouldBe(BigInt.cbrt(-125n), -5n);
shouldBe(BigInt.cbrt(1_881_676_371_789_154_860_897_069n), 123_456_789n);
shouldBe(BigInt.cbrt(-1_881_676_371_789_154_860_897_069n), -123_456_789n);
shouldBe(BigInt.cbrt(1_881_676_377_434_183_981_909_562_699_940_347_954_480_361_860_897_069n), 123_456_789_123_456_789n);
shouldBe(BigInt.cbrt(-1_881_676_377_434_183_981_909_562_699_940_347_954_480_361_860_897_069n), -123_456_789_123_456_789n);

// Heap values around 2^53 where a `Number`-based `Math.cbrt` would be off-by-one;
// these exercise the exact integer cube-root path (including a perfect cube).
shouldBe(BigInt.cbrt(9_007_091_372_906_046n), 208_062n);
shouldBe(BigInt.cbrt(9_007_091_372_906_047n), 208_063n);
shouldBe(BigInt.cbrt(9_007_091_372_906_048n), 208_063n);
shouldBe(BigInt.cbrt(-9_007_091_372_906_046n), -208_062n);
shouldBe(BigInt.cbrt(-9_007_091_372_906_047n), -208_063n);
shouldBe(BigInt.cbrt(-9_007_091_372_906_048n), -208_063n);
shouldBe(BigInt.cbrt(1_881_676_377_413_297_425_320_704_533_203_867n), 123_456_789_123n);
shouldBe(BigInt.cbrt(-1_881_676_377_413_297_425_320_704_533_203_867n), -123_456_789_123n);
