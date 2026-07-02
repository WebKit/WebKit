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

shouldThrow(() => BigInt.pow(), `TypeError: BigInt.pow requires the first argument to be a BigInt`);
for (let value of [ false, true, -1, 0, 1, "-1", "0", "1", undefined, null, [ ], { } ]) {
    shouldThrow(() => BigInt.pow(value), `TypeError: BigInt.pow requires the first argument to be a BigInt`);
    shouldThrow(() => BigInt.pow(1n, value), `TypeError: BigInt.pow requires the second argument to be a BigInt`);
    shouldThrow(() => BigInt.pow(value, 2n), `TypeError: BigInt.pow requires the first argument to be a BigInt`);
    shouldBe(BigInt.pow(2n, 3n, value), 8n);
    shouldBe(BigInt.pow(-123_456_789n, 2n, value), 15_241_578_750_190_521n);
}

shouldThrow(() => BigInt.pow(1n, -1n), `RangeError: Negative exponent is not allowed`);
shouldThrow(() => BigInt.pow(-1n, -1n), `RangeError: Negative exponent is not allowed`);
shouldThrow(() => BigInt.pow(2n, -2n), `RangeError: Negative exponent is not allowed`);
shouldThrow(() => BigInt.pow(-2n, -2n), `RangeError: Negative exponent is not allowed`);

shouldBe(BigInt.pow(0n, 0n), 1n);
shouldBe(BigInt.pow(-0n, 0n), 1n);
shouldBe(BigInt.pow(0n, -0n), 1n);
shouldBe(BigInt.pow(-0n, -0n), 1n);

shouldBe(BigInt.pow(1n, 1n), 1n);
shouldBe(BigInt.pow(-1n, 1n), -1n);

shouldBe(BigInt.pow(2n, 2n), 4n);
shouldBe(BigInt.pow(-2n, 2n), 4n);

shouldBe(BigInt.pow(2n, 3n), 8n);
shouldBe(BigInt.pow(-2n, 3n), -8n);

shouldBe(BigInt.pow(3n, 2n), 9n);
shouldBe(BigInt.pow(-3n, 2n), 9n);

shouldBe(BigInt.pow(3n, 3n), 27n);
shouldBe(BigInt.pow(-3n, 3n), -27n);

shouldBe(BigInt.pow(2n, 4n), 16n);
shouldBe(BigInt.pow(-2n, 4n), 16n);

shouldBe(BigInt.pow(3n, 4n), 81n);
shouldBe(BigInt.pow(-3n, 4n), 81n);

shouldBe(BigInt.pow(4n, 4n), 256n);
shouldBe(BigInt.pow(-4n, 4n), 256n);

shouldBe(BigInt.pow(123_456_789n, 2n), 15_241_578_750_190_521n);
shouldBe(BigInt.pow(-123_456_789n, 2n), 15_241_578_750_190_521n);

shouldBe(BigInt.pow(123_456_789n, 3n), 1_881_676_371_789_154_860_897_069n);
shouldBe(BigInt.pow(-123_456_789n, 3n), -1_881_676_371_789_154_860_897_069n);

shouldBe(BigInt.pow(123_456_789_123_456_789n, 2n), 15_241_578_780_673_678_515_622_620_750_190_521n);
shouldBe(BigInt.pow(-123_456_789_123_456_789n, 2n), 15_241_578_780_673_678_515_622_620_750_190_521n);

shouldBe(BigInt.pow(123_456_789_123_456_789n, 3n), 1_881_676_377_434_183_981_909_562_699_940_347_954_480_361_860_897_069n);
shouldBe(BigInt.pow(-123_456_789_123_456_789n, 3n), -1_881_676_377_434_183_981_909_562_699_940_347_954_480_361_860_897_069n);

shouldBe(BigInt.pow(0n, 123_456_789n), 0n);
shouldBe(BigInt.pow(-0n, 123_456_789n), 0n);

shouldBe(BigInt.pow(0n, 123_456_789_123_456_789n), 0n);
shouldBe(BigInt.pow(-0n, 123_456_789_123_456_789n), 0n);

shouldBe(BigInt.pow(1n, 123_456_789n), 1n);
shouldBe(BigInt.pow(-1n, 123_456_789n), -1n);

shouldBe(BigInt.pow(1n, 123_456_789_123_456_789n), 1n);
shouldBe(BigInt.pow(-1n, 123_456_789_123_456_789n), -1n);
