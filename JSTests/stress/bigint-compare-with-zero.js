function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

const zero = BigInt(0);

function compareRight(x) {
    return [x < zero, x <= zero, x > zero, x >= zero, x == zero, x === zero, x != zero, x !== zero];
}

function compareLeft(x) {
    return [zero < x, zero <= x, zero > x, zero >= x, zero == x, zero === x, zero != x, zero !== x];
}

function branchRight(x) {
    let result = 0;
    if (x < zero)
        result |= 1;
    if (x <= zero)
        result |= 2;
    if (x > zero)
        result |= 4;
    if (x >= zero)
        result |= 8;
    if (x === zero)
        result |= 16;
    return result;
}
noInline(compareRight);
noInline(compareLeft);
noInline(branchRight);

const big = 2n ** 200n;
// Zeros reached through different operations, all of which must be the same HeapBigInt.
const zeros = [big - big, BigInt("0"), BigInt("0x0"), big % big, big >> 300n, BigInt.asIntN(8, 256n), big & (big - 1n), -(big - big)];
const values = [0n, 1n, -1n, big, -big, -(2n ** 64n), 2n ** 64n, ...zeros];

function expectedRight(x) {
    let sign = x > 0n ? 1 : x < 0n ? -1 : 0;
    return [sign < 0, sign <= 0, sign > 0, sign >= 0, sign === 0, sign === 0, sign !== 0, sign !== 0];
}

function expectedLeft(x) {
    let sign = x > 0n ? 1 : x < 0n ? -1 : 0;
    return [0 < sign, 0 <= sign, 0 > sign, 0 >= sign, sign === 0, sign === 0, sign !== 0, sign !== 0];
}

for (let i = 0; i < testLoopCount; ++i) {
    const value = values[i % values.length];
    const x = i & 1 ? value * big : value;
    const right = compareRight(x);
    const left = compareLeft(x);
    const expectedR = expectedRight(x);
    const expectedL = expectedLeft(x);
    for (let j = 0; j < right.length; ++j) {
        shouldBe(right[j], expectedR[j]);
        shouldBe(left[j], expectedL[j]);
    }
    let expectedBranch = (expectedR[0] ? 1 : 0) | (expectedR[1] ? 2 : 0) | (expectedR[2] ? 4 : 0) | (expectedR[3] ? 8 : 0) | (expectedR[5] ? 16 : 0);
    shouldBe(branchRight(x), expectedBranch);
}
