function assert(a, message) {
    if (!a)
        throw new Error(message);
}

function assertThrowRangeError(a, b) {
    try {
        let n = a ** b;
        assert(false, "Should throw RangeError, but executed without exception");
    } catch (e) {
        assert(e instanceof RangeError, "Expected RangeError, got: " + e);
    }
}

assertThrowRangeError(1n, -1n);
assertThrowRangeError(0n, -1n);
assertThrowRangeError(-1n, -1n);
assertThrowRangeError(1n, -100000000000000000n);

