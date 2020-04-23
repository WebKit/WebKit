function assert(b) {
    if (!b)
        throw new Error;
}

function assertIsBigInt32(arg) {
    if (useBigInt32())
        assert(isBigInt32(arg));
    else
        assert(isHeapBigInt(arg));
}

assertIsBigInt32(2147483647n);
assertIsBigInt32(2147483646n);
assertIsBigInt32(2127483646n);
assertIsBigInt32(1127483646n);
assertIsBigInt32(-2147483648n);
assertIsBigInt32(-2147483647n);
assertIsBigInt32(-1147483647n);
assertIsBigInt32(0n);
assertIsBigInt32(1n);
assertIsBigInt32(-1n);
assertIsBigInt32(42n);

assert(isHeapBigInt(2147483648n));
assert(isHeapBigInt(-2147483649n));
assert(isHeapBigInt(3147483648n));
assert(isHeapBigInt(9147483648n));
assert(isHeapBigInt(-9147483649n));
assert(isHeapBigInt(-2147583649n));
