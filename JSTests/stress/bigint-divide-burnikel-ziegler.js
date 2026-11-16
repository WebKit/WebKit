// Burnikel-Ziegler division splits the divisor into a power-of-two number of equal blocks, pads it up
// to a whole number of them, and divides the dividend block by block, so the shapes that matter are
// the divisor sizes where the block count or the padding changes, and the dividend sizes that end
// a block just before or after its boundary. Each quotient and remainder is checked against
// x == q * y + r with 0 <= r < y, which relies on the multiplication paths but shares no division
// code, and the quotient of exact multiples is checked directly.
//
// A Digit is a CPU register, so a given bit width is one digit count on 64-bit targets and twice
// that on 32-bit ones. Both widths are exercised so the same digit counts are covered either way.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected.toString(16).slice(0, 40)}... but got ${actual.toString(16).slice(0, 40)}...`);
}

function makeOperand(width, digits, seed, shape) {
    const hexDigits = width / 4;
    const mask = (1n << BigInt(width)) - 1n;
    const shift = BigInt(64 - width);
    const parts = new Array(digits);
    let mix = BigInt.asUintN(64, 0x9e3779b97f4a7c15n * BigInt(seed + 1));
    for (let i = 0; i < digits; i++) {
        mix = BigInt.asUintN(64, mix * 6364136223846793005n + 1442695040888963407n);
        let digit;
        switch (shape) {
        case "random":
            digit = mix >> shift;
            break;
        case "ones":
            digit = mask;
            break;
        case "sparse":
            digit = (i * 7 + seed) % 5 === 0 ? mix >> shift : 0n;
            break;
        case "top":
            // Only the top digit is set, with its high bit, so the divisor needs no normalization
            // shift and the dividend's top block is maximal.
            digit = i ? 0n : 1n << BigInt(width - 1);
            break;
        case "low":
            // A small top digit forces the largest normalization shift.
            digit = i ? mix >> shift : 1n;
            break;
        }
        parts[i] = digit.toString(16).padStart(hexDigits, "0");
    }
    if (shape === "random" || shape === "sparse")
        parts[0] = "8" + parts[0].slice(1);
    return BigInt("0x" + parts.join(""));
}

function check(x, y, message) {
    const q = x / y;
    const r = x % y;
    if (r < 0n || r >= y)
        throw new Error(`${message}: remainder out of range`);
    shouldBe(q * y + r, x, `${message} identity`);
    shouldBe((-x) / (-y), q, `${message} negative operands quotient`);
    shouldBe((-x) % y, -r, `${message} negative dividend remainder`);
}

// The recursion stops below 16 digits, division only takes it for a divisor of at least 24 digits and
// a quotient of at least 48, and the number of blocks doubles each time the divisor passes a multiple
// of 16 by a power of two.
const minDivisorSize = 24;
const minQuotientSize = 48;
const leafSize = 16;
const shapes = ["random", "ones", "sparse", "top", "low"];

for (const width of [32, 64]) {
    // Divisor sizes around the smallest one that takes the recursion and each doubling of the leaf
    // size, where it gains a level, with dividends from one digit longer up to many blocks, and
    // around the smallest quotient that takes it. The mix of shapes covers both branches
    // of the three-part split, where the top of the dividend is or is not below the top of the
    // divisor, and the correction loop after it.
    for (const divisorSize of [minDivisorSize - 1, minDivisorSize, minDivisorSize + 1, 2 * leafSize, 2 * leafSize + 1, 4 * leafSize, 4 * leafSize + 1, 8 * leafSize + 1, 16 * leafSize + 1]) {
        for (const extra of [1, minQuotientSize - 2, minQuotientSize - 1, divisorSize, 2 * divisorSize + 1]) {
            const dividendSize = divisorSize + extra;
            for (const shape of shapes) {
                const x = makeOperand(width, dividendSize, dividendSize, shape);
                const y = makeOperand(width, divisorSize, divisorSize * 3 + 1, shapes[(shapes.indexOf(shape) + 1) % shapes.length]);
                check(x, y, `${width} bit digits, ${dividendSize} / ${divisorSize} ${shape}`);
            }
        }
    }

    // Exact multiples, and the remainders 1 and y - 1, with quotients of various sizes: the quotient
    // has no partial top block, and the remainder is the smallest and largest one the last block can
    // produce.
    for (const divisorSize of [minDivisorSize, 128, 1000]) {
        const y = makeOperand(width, divisorSize, divisorSize, "random");
        for (const quotientSize of [1, 2, minQuotientSize, 300]) {
            const q = makeOperand(width, quotientSize, quotientSize * 7, "sparse");
            for (const r of [0n, 1n, y - 1n]) {
                const x = q * y + r;
                const label = `${width} bit digits, ${quotientSize} x ${divisorSize} + ${r === 0n ? "0" : r === 1n ? "1" : "y - 1"}`;
                shouldBe(x / y, q, `${label} quotient`);
                shouldBe(x % y, r, `${label} remainder`);
            }
        }
    }

    // Dividends just below, and just under a multiple of, the divisor at every alignment relative to
    // the blocks. The top of such a dividend equals the top of the divisor, which takes the branch
    // of the three-part split where the quotient estimate starts from its maximum and has to be
    // corrected downwards, and the remainders sit near 0 and near y.
    for (const divisorSize of [minDivisorSize, minDivisorSize + 1, 4 * leafSize, 8 * leafSize]) {
        const y = makeOperand(width, divisorSize, divisorSize + 5, "random");
        const bits = width * divisorSize;
        for (const shift of [0, 1, width - 1, width, width + 1, bits / 2, bits, bits + 1, bits + width, 2 * bits + 3]) {
            check(((y - 1n) << BigInt(shift)) + BigInt(shift), y, `${width} bit digits, (y - 1) << ${shift} of ${divisorSize}`);
            check((y << BigInt(shift)) - 1n, y, `${width} bit digits, (y << ${shift}) - 1 of ${divisorSize}`);
        }
    }

    // Powers of two as divisors and dividends: a divisor that is a whole number of digits needs no
    // normalization shift at all.
    for (const bits of [width * minDivisorSize, width * 1000 + 1]) {
        const p = 1n << BigInt(bits);
        const x = makeOperand(width, Math.ceil(bits / width) * 2 + 3, bits, "random");
        shouldBe(x / p, x >> BigInt(bits), `${bits} bit power of two divisor`);
        shouldBe(x % p, x & (p - 1n), `${bits} bit power of two remainder`);
        shouldBe(x / (p - 1n) * (p - 1n) + x % (p - 1n), x, `${bits} bit all ones divisor`);
        shouldBe((p * p) / p, p, `${bits} bit power of two dividend`);
        shouldBe((p * p - 1n) / p, p - 1n, `${bits} bit all ones dividend`);
        shouldBe((p * p - 1n) % p, p - 1n, `${bits} bit all ones dividend remainder`);
    }
}
