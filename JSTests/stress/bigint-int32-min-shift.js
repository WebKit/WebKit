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

let int32min = -0x7fffffffn - 1n;
shouldBe(0n >> int32min, 0n);
shouldBe(0n >> (int32min + 1n), 0n);
shouldBe(0n << int32min, 0n);
shouldBe(0n << (int32min + 1n), 0n);
shouldBe(1n << int32min, 0n);
shouldBe(1n << (int32min + 1n), 0n);
shouldBe(-1n << int32min, -1n);
shouldBe(-1n << (int32min + 1n), -1n);
shouldBe(0x7fffffffn << int32min, 0n);
shouldBe(0x7fffffffn << (int32min + 1n), 0n);
shouldBe(0x7fffffffffffn << int32min, 0n);
shouldBe(0x7fffffffffffn << (int32min + 1n), 0n);
shouldBe(-0x7fffffffn << int32min, -1n);
shouldBe(-0x7fffffffn << (int32min + 1n), -1n);
shouldBe(-0x7fffffffffffn << int32min, -1n);
shouldBe(-0x7fffffffffffn << (int32min + 1n), -1n);
shouldThrow(() => {
    1n >> int32min;
}, `RangeError: Out of memory: BigInt generated from this operation is too big`);
shouldThrow(() => {
    -1n >> int32min;
}, `RangeError: Out of memory: BigInt generated from this operation is too big`);
shouldThrow(() => {
    0x7fffffffn >> int32min;
}, `RangeError: Out of memory: BigInt generated from this operation is too big`);
shouldThrow(() => {
    (-0x7fffffffn - 1n) >> int32min;
}, `RangeError: Out of memory: BigInt generated from this operation is too big`);
