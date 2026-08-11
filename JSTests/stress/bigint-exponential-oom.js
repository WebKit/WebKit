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
    2n ** 0xfffffffffffffffffn;
}, `RangeError: Out of memory: BigInt generated from this operation is too big`);
shouldThrow(() => {
    2n ** 0xffffffn;
}, `RangeError: Out of memory: BigInt generated from this operation is too big`);
shouldThrow(() => {
    2n ** 0xfffffffn;
}, `RangeError: Out of memory: BigInt generated from this operation is too big`);
shouldThrow(() => {
    2n ** 0xffffffffn;
}, `RangeError: Out of memory: BigInt generated from this operation is too big`);
shouldThrow(() => {
    2n ** 0xfffffffffffffffn;
}, `RangeError: Out of memory: BigInt generated from this operation is too big`);
shouldThrow(() => {
    10n ** 1000000n;
}, `RangeError: Out of memory: BigInt generated from this operation is too big`);
