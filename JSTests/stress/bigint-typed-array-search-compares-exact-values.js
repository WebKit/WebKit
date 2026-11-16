// includes, indexOf, and lastIndexOf on BigInt typed arrays compare exact values, so a BigInt
// outside the element type's range is never found.

function check(actual, expected, what) {
    if (actual !== expected)
        throw new Error(what + ": expected " + expected + " but got " + actual);
}

const unsigned = new BigUint64Array([1n, 2n ** 64n - 1n, 0n]);
check(unsigned.includes(-1n), false, "BigUint64Array includes -1n");
check(unsigned.indexOf(-1n), -1, "BigUint64Array indexOf -1n");
check(unsigned.lastIndexOf(-1n), -1, "BigUint64Array lastIndexOf -1n");
check(unsigned.indexOf(2n ** 64n), -1, "BigUint64Array indexOf 2n ** 64n");
check(unsigned.indexOf(2n ** 64n - 1n), 1, "BigUint64Array indexOf its maximum");
check(unsigned.indexOf(0n), 2, "BigUint64Array indexOf 0n");

const signed = new BigInt64Array([-1n, 5n, -(2n ** 63n), 2n ** 63n - 1n]);
check(signed.includes(2n ** 64n - 1n), false, "BigInt64Array includes 2n ** 64n - 1n");
check(signed.indexOf(5n + 2n ** 64n), -1, "BigInt64Array indexOf 5n + 2n ** 64n");
check(signed.indexOf(2n ** 63n), -1, "BigInt64Array indexOf 2n ** 63n");
check(signed.indexOf(-(2n ** 63n) - 1n), -1, "BigInt64Array indexOf below its minimum");
check(signed.indexOf(-(2n ** 63n)), 2, "BigInt64Array indexOf its minimum");
check(signed.indexOf(2n ** 63n - 1n), 3, "BigInt64Array indexOf its maximum");
check(signed.lastIndexOf(-1n), 0, "BigInt64Array lastIndexOf -1n");
