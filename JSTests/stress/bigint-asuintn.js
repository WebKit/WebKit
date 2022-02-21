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
    BigInt.asUintN(-1, 0n)
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
    shouldBe(BigInt.asUintN(index, bigint), 10n);
    shouldBe(toIndex, true);
    shouldBe(toBigInt, true);
}

shouldBe(BigInt.asUintN(-0, 0n), 0n);
shouldBe(BigInt.asUintN(0, 0n), 0n);
shouldBe(BigInt.asUintN(1, 0n), 0n);
shouldBe(BigInt.asUintN(2, 0n), 0n);
shouldBe(BigInt.asUintN(3, 0n), 0n);

shouldBe(BigInt.asUintN(0, 1n), 0n);
shouldBe(BigInt.asUintN(1, 1n), 1n);
shouldBe(BigInt.asUintN(2, 1n), 1n);
shouldBe(BigInt.asUintN(3, 1n), 1n);

shouldBe(BigInt.asUintN(30, 0x7fffffffn), 1073741823n);
shouldBe(BigInt.asUintN(31, 0x7fffffffn), 2147483647n);
shouldBe(BigInt.asUintN(32, 0x7fffffffn), 2147483647n);
shouldBe(BigInt.asUintN(33, 0x7fffffffn), 2147483647n);
shouldBe(BigInt.asUintN(34, 0x7fffffffn), 2147483647n);

shouldBe(BigInt.asUintN(30, 0x80000000n), 0n);
shouldBe(BigInt.asUintN(31, 0x80000000n), 0n);
shouldBe(BigInt.asUintN(32, 0x80000000n), 2147483648n);
shouldBe(BigInt.asUintN(33, 0x80000000n), 2147483648n);
shouldBe(BigInt.asUintN(34, 0x80000000n), 2147483648n);

shouldBe(BigInt.asUintN(30, 0xffffffffn), 1073741823n);
shouldBe(BigInt.asUintN(31, 0xffffffffn), 2147483647n);
shouldBe(BigInt.asUintN(32, 0xffffffffn), 4294967295n);
shouldBe(BigInt.asUintN(33, 0xffffffffn), 4294967295n);
shouldBe(BigInt.asUintN(34, 0xffffffffn), 4294967295n);

shouldBe(BigInt.asUintN(30, -0x80000000n), 0n);
shouldBe(BigInt.asUintN(31, -0x80000000n), 0n);
shouldBe(BigInt.asUintN(32, -0x80000000n), 2147483648n);
shouldBe(BigInt.asUintN(33, -0x80000000n), 6442450944n);
shouldBe(BigInt.asUintN(34, -0x80000000n), 15032385536n);

shouldBe(BigInt.asUintN(30, -0x80000001n), 1073741823n);
shouldBe(BigInt.asUintN(31, -0x80000001n), 2147483647n);
shouldBe(BigInt.asUintN(32, -0x80000001n), 2147483647n);
shouldBe(BigInt.asUintN(33, -0x80000001n), 6442450943n);
shouldBe(BigInt.asUintN(34, -0x80000001n), 15032385535n);

shouldBe(BigInt.asUintN(30, -0xffffffffn), 1n);
shouldBe(BigInt.asUintN(31, -0xffffffffn), 1n);
shouldBe(BigInt.asUintN(32, -0xffffffffn), 1n);
shouldBe(BigInt.asUintN(33, -0xffffffffn), 4294967297n);
shouldBe(BigInt.asUintN(34, -0xffffffffn), 12884901889n);

shouldBe(BigInt.asUintN(62, 0x7fffffffffffffffn), 4611686018427387903n);
shouldBe(BigInt.asUintN(63, 0x7fffffffffffffffn), 9223372036854775807n);
shouldBe(BigInt.asUintN(64, 0x7fffffffffffffffn), 9223372036854775807n);
shouldBe(BigInt.asUintN(65, 0x7fffffffffffffffn), 9223372036854775807n);
shouldBe(BigInt.asUintN(66, 0x7fffffffffffffffn), 9223372036854775807n);

shouldBe(BigInt.asUintN(62, 0x8000000000000000n), 0n);
shouldBe(BigInt.asUintN(63, 0x8000000000000000n), 0n);
shouldBe(BigInt.asUintN(64, 0x8000000000000000n), 9223372036854775808n);
shouldBe(BigInt.asUintN(65, 0x8000000000000000n), 9223372036854775808n);
shouldBe(BigInt.asUintN(66, 0x8000000000000000n), 9223372036854775808n);

shouldBe(BigInt.asUintN(62, 0xffffffffffffffffn), 4611686018427387903n);
shouldBe(BigInt.asUintN(63, 0xffffffffffffffffn), 9223372036854775807n);
shouldBe(BigInt.asUintN(64, 0xffffffffffffffffn), 18446744073709551615n);
shouldBe(BigInt.asUintN(65, 0xffffffffffffffffn), 18446744073709551615n);
shouldBe(BigInt.asUintN(66, 0xffffffffffffffffn), 18446744073709551615n);

shouldBe(BigInt.asUintN(62, -0x800000000000000n), 4035225266123964416n);
shouldBe(BigInt.asUintN(63, -0x800000000000000n), 8646911284551352320n);
shouldBe(BigInt.asUintN(64, -0x800000000000000n), 17870283321406128128n);
shouldBe(BigInt.asUintN(65, -0x800000000000000n), 36317027395115679744n);
shouldBe(BigInt.asUintN(66, -0x800000000000000n), 73210515542534782976n);

shouldBe(BigInt.asUintN(62, -0x800000000000001n), 4035225266123964415n);
shouldBe(BigInt.asUintN(63, -0x800000000000001n), 8646911284551352319n);
shouldBe(BigInt.asUintN(64, -0x800000000000001n), 17870283321406128127n);
shouldBe(BigInt.asUintN(65, -0x800000000000001n), 36317027395115679743n);
shouldBe(BigInt.asUintN(66, -0x800000000000001n), 73210515542534782975n);

shouldBe(BigInt.asUintN(62, -0xffffffffffffffffn), 1n);
shouldBe(BigInt.asUintN(63, -0xffffffffffffffffn), 1n);
shouldBe(BigInt.asUintN(64, -0xffffffffffffffffn), 1n);
shouldBe(BigInt.asUintN(65, -0xffffffffffffffffn), 18446744073709551617n);
shouldBe(BigInt.asUintN(66, -0xffffffffffffffffn), 55340232221128654849n);
shouldBe(BigInt.asUintN(67, -0xffffffffffffffffn), 129127208515966861313n);
