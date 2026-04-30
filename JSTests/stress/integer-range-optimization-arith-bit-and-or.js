// Exercises the ArithBitAnd / ArithBitOr rules in DFGIntegerRangeOptimizationPhase.
// Only constant operands are used to derive bounds, because constants never change
// across the phase's fixpoint iterations. Using rangeFor() on non-constant operands
// would let stale facts accumulate on the result node.

function assert(actual, expected) {
    if (actual !== expected)
        throw new Error("Expected " + expected + " but got " + actual);
}

// ArithBitAnd with a non-negative constant: (x & 0xFF) is in [0, 255],
// so the array index is always in bounds.
function bitAndIndex(arr, x) {
    return arr[x & 0xFF];
}
noInline(bitAndIndex);

// ArithBitAnd with the constant on the left side.
function bitAndIndexLeftConst(arr, x) {
    return arr[0xFF & x];
}
noInline(bitAndIndexLeftConst);

// ArithBitAnd composed: inner result is [0, 0x7F], outer result is bounded by 0x0F.
function bitAndTwoMasks(x, y) {
    var a = x & 0x7F;
    var b = y & 0x0F;
    return (a & b) + 1;
}
noInline(bitAndTwoMasks);

// ArithBitAnd with a negative constant: rule must not claim any bounds.
// (x & -1) == x for all x, including negative.
function bitAndPreserveNegative(x) {
    return x & -1;
}
noInline(bitAndPreserveNegative);

// ArithBitOr with a negative constant: result is always negative.
function bitOrWithNegative(x) {
    return (x & 0xFF) | -1;
}
noInline(bitOrWithNegative);

// ArithBitOr with a non-negative constant: rule derives nothing (since the
// non-constant side could be negative). Correctness must still hold.
function bitOrZero(arr, x) {
    return arr[(x & 0xFF) | 0];
}
noInline(bitOrZero);

// ArithBitOr with a non-negative constant on the left.
function bitOrOneLeft(x) {
    return 1 | (x & 0xFF);
}
noInline(bitOrOneLeft);

// ArithBitOr with a negative constant establishes a lower bound: (x | C) >= C
// for C < 0. Using the result of (y & 0xFF) | -16 as an index requires the
// optimizer to reason about both sides of the range.
function bitOrNegativeLowerBound(x) {
    return (x & 0xFF) | -16;
}
noInline(bitOrNegativeLowerBound);

// ArithBitOr with INT32_MIN: result has the sign bit set, so it is negative,
// but the lower-bound rule must not underflow while computing C - 1.
function bitOrIntMin(x) {
    return (x & 0xFF) | (-2147483647 - 1);
}
noInline(bitOrIntMin);

var buf = new Array(256);
for (var i = 0; i < 256; ++i)
    buf[i] = i;

for (var i = 0; i < testLoopCount; ++i) {
    assert(bitAndIndex(buf, i), i & 0xFF);
    assert(bitAndIndex(buf, -i), (-i) & 0xFF);

    assert(bitAndIndexLeftConst(buf, i), i & 0xFF);
    assert(bitAndIndexLeftConst(buf, -i), (-i) & 0xFF);

    assert(bitAndTwoMasks(i, i * 3), ((i & 0x7F) & ((i * 3) & 0x0F)) + 1);

    assert(bitAndPreserveNegative(i), i);
    assert(bitAndPreserveNegative(-i), -i);
    assert(bitAndPreserveNegative(-1), -1);

    assert(bitOrWithNegative(i), -1);
    assert(bitOrWithNegative(-i), -1);

    assert(bitOrZero(buf, i), i & 0xFF);
    assert(bitOrZero(buf, -i), (-i) & 0xFF);

    assert(bitOrOneLeft(i), 1 | (i & 0xFF));
    assert(bitOrOneLeft(0), 1);

    assert(bitOrNegativeLowerBound(i), (i & 0xFF) | -16);
    assert(bitOrNegativeLowerBound(-i), ((-i) & 0xFF) | -16);

    assert(bitOrIntMin(i), (i & 0xFF) | -2147483648);
    assert(bitOrIntMin(-i), ((-i) & 0xFF) | -2147483648);
}

