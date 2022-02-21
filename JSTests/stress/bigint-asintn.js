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

shouldThrow(() => {
    BigInt.asIntN(-1, 0n)
}, `RangeError: number of bits cannot be negative`);

{
    let toIndex = false;
    let toBigInt = false;
    let index = {
        [Symbol.toPrimitive](hint) {
            shouldBe(hint, "number");
            shouldBe(toIndex, false);
            shouldBe(toBigInt, false);
            toIndex = true;
            return 32;
        }
    };
    let bigint = {
        [Symbol.toPrimitive](hint) {
            shouldBe(hint, "number");
            shouldBe(toIndex, true);
            shouldBe(toBigInt, false);
            toBigInt = true;
            return 10n;
        }
    }
    shouldBe(BigInt.asIntN(index, bigint), 10n);
    shouldBe(toIndex, true);
    shouldBe(toBigInt, true);
}

shouldBe(BigInt.asIntN(-0, 0n), 0n);

shouldBe(BigInt.asIntN(0, 0n), 0n);
shouldBe(BigInt.asIntN(1, 0n), 0n);
shouldBe(BigInt.asIntN(2, 0n), 0n);
shouldBe(BigInt.asIntN(3, 0n), 0n);

shouldBe(BigInt.asIntN(0, 1n), 0n);
shouldBe(BigInt.asIntN(1, 1n), -1n);
shouldBe(BigInt.asIntN(2, 1n), 1n);
shouldBe(BigInt.asIntN(3, 1n), 1n);

shouldBe(BigInt.asIntN(30, 0x7fffffffn), -1n);
shouldBe(BigInt.asIntN(31, 0x7fffffffn), -1n);
shouldBe(BigInt.asIntN(32, 0x7fffffffn), 2147483647n);
shouldBe(BigInt.asIntN(33, 0x7fffffffn), 2147483647n);
shouldBe(BigInt.asIntN(34, 0x7fffffffn), 2147483647n);

shouldBe(BigInt.asIntN(30, 0x80000000n), 0n);
shouldBe(BigInt.asIntN(31, 0x80000000n), 0n);
shouldBe(BigInt.asIntN(32, 0x80000000n), -2147483648n);
shouldBe(BigInt.asIntN(33, 0x80000000n), 2147483648n);
shouldBe(BigInt.asIntN(34, 0x80000000n), 2147483648n);

shouldBe(BigInt.asIntN(30, -0x80000000n), 0n);
shouldBe(BigInt.asIntN(31, -0x80000000n), 0n);
shouldBe(BigInt.asIntN(32, -0x80000000n), -2147483648n);
shouldBe(BigInt.asIntN(33, -0x80000000n), -2147483648n);
shouldBe(BigInt.asIntN(34, -0x80000000n), -2147483648n);

shouldBe(BigInt.asIntN(30, -0x80000001n), -1n);
shouldBe(BigInt.asIntN(31, -0x80000001n), -1n);
shouldBe(BigInt.asIntN(32, -0x80000001n), 2147483647n);
shouldBe(BigInt.asIntN(33, -0x80000001n), -2147483649n);
shouldBe(BigInt.asIntN(34, -0x80000001n), -2147483649n);

shouldBe(BigInt.asIntN(62, 0x7fffffffffffffffn), -1n);
shouldBe(BigInt.asIntN(63, 0x7fffffffffffffffn), -1n);
shouldBe(BigInt.asIntN(64, 0x7fffffffffffffffn), 9223372036854775807n);
shouldBe(BigInt.asIntN(65, 0x7fffffffffffffffn), 9223372036854775807n);
shouldBe(BigInt.asIntN(66, 0x7fffffffffffffffn), 9223372036854775807n);

shouldBe(BigInt.asIntN(62, 0x8000000000000000n), 0n);
shouldBe(BigInt.asIntN(63, 0x8000000000000000n), 0n);
shouldBe(BigInt.asIntN(64, 0x8000000000000000n), -9223372036854775808n);
shouldBe(BigInt.asIntN(65, 0x8000000000000000n), 9223372036854775808n);
shouldBe(BigInt.asIntN(66, 0x8000000000000000n), 9223372036854775808n);

shouldBe(BigInt.asIntN(62, -0x800000000000000n), -576460752303423488n);
shouldBe(BigInt.asIntN(63, -0x800000000000000n), -576460752303423488n);
shouldBe(BigInt.asIntN(64, -0x800000000000000n), -576460752303423488n);
shouldBe(BigInt.asIntN(65, -0x800000000000000n), -576460752303423488n);
shouldBe(BigInt.asIntN(66, -0x800000000000000n), -576460752303423488n);

shouldBe(BigInt.asIntN(62, -0x800000000000001n), -576460752303423489n);
shouldBe(BigInt.asIntN(63, -0x800000000000001n), -576460752303423489n);
shouldBe(BigInt.asIntN(64, -0x800000000000001n), -576460752303423489n);
shouldBe(BigInt.asIntN(65, -0x800000000000001n), -576460752303423489n);
shouldBe(BigInt.asIntN(66, -0x800000000000001n), -576460752303423489n);
