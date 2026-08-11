function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

// multiplyImpl routes a product to one of three implementations depending on the operand shape:
// a fully unrolled fixed-size form for equal small sizes, product scanning (Comba), or the
// schoolbook loop. Comba's column walk is split into ramp-up, steady and ramp-down phases whose
// bounds differ per phase, so the shapes that matter are the ones that straddle those phase
// boundaries and the ones that sit either side of the routing thresholds.
//
// A Digit is a CPU register, so a given bit width is one digit count on 64-bit targets and twice
// that on 32-bit ones. Both widths are exercised so the same digit counts are covered either way.

// Reference multiply built from shifts and adds, which never reaches the multiply routines under
// test for the operand sizes below.
function refMul(a, b) {
    let result = 0n;
    let shift = 0n;
    while (b > 0n) {
        // 16 bits at a time keeps every intermediate product inside one digit.
        const chunk = b & 0xffffn;
        if (chunk)
            result += (a * chunk) << shift;
        b >>= 16n;
        shift += 16n;
    }
    return result;
}

function makeOperand(digits, width, seed) {
    let mix = BigInt.asUintN(width, 0x9e3779b97f4a7c15n * BigInt(seed + 1));
    const mask = (1n << BigInt(width)) - 1n;
    let value = 0n;
    for (let i = 0; i < digits; i++) {
        mix = BigInt.asUintN(width, mix * 6364136223846793005n + 1442695040888963407n);
        value |= (mix & mask) << BigInt(width * i);
    }
    // Force the top digit non-zero so the operand really has `digits` digits.
    return value | (1n << BigInt(width * digits - 1));
}

function check(x, y) {
    const expected = refMul(x, y);
    shouldBe(x * y, expected);
    shouldBe(y * x, expected);
    shouldBe((-x) * y, -expected);
    shouldBe((-x) * (-y), expected);
}

for (const width of [32, 64]) {
    // Every shape up to 20x20 covers each phase boundary of the column walk, both sides of all
    // four routing thresholds, and the degenerate single-digit and equal-size cases.
    for (let larger = 1; larger <= 20; larger++) {
        for (let smaller = 1; smaller <= larger; smaller++) {
            const x = makeOperand(larger, width, larger * 31 + smaller);
            const y = makeOperand(smaller, width, smaller * 17 + larger);
            check(x, y);
        }
    }

    // Wide shapes with a thin second operand, where the ramp-down phase is longest relative to the
    // rest of the walk.
    for (const larger of [24, 32, 40, 64]) {
        for (const smaller of [1, 2, 3, 4, 5, 8, 16]) {
            const x = makeOperand(larger, width, larger + smaller);
            const y = makeOperand(smaller, width, larger * smaller);
            check(x, y);
        }
    }

    // All-ones operands maximize every column sum, so the three-digit running total carries as far
    // as it can at each step.
    for (const larger of [2, 3, 5, 8, 13, 16, 32]) {
        for (const smaller of [1, 2, 3, 5, 8, 16]) {
            if (smaller > larger)
                continue;
            const x = (1n << BigInt(width * larger)) - 1n;
            const y = (1n << BigInt(width * smaller)) - 1n;
            check(x, y);
        }
    }

    // Squaring aliases the two operands.
    for (const digits of [1, 2, 3, 4, 5, 8, 13, 16, 17, 32]) {
        const x = makeOperand(digits, width, digits);
        check(x, x);
    }
}
