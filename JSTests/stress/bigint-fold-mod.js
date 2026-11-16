function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

// Divisors close to a power of the digit base reduce by folding the high half of the dividend down
// with a single-digit multiply instead of using the cached multiplicative inverse. Only divisors
// whose reduction factor is a single digit and whose quotient into that power of two is small
// qualify, so cover both sides of that gate and confirm the answers agree with a reference path.
//
// The cache is only armed after 100 uses of the same divisor, so each case loops past that
// threshold to exercise the pre-arming and post-arming paths.
//
// A Digit is a CPU register, so it is 64 bits on 64-bit targets and 32 bits on 32-bit ones. Cases
// built from a digit width run at both so the qualifying shape is hit either way.
const digitWidths = [32, 64];

// Reference modulo that does not go through the cached path.
function refMod(a, b) {
    return a - (a / b) * b;
}

function checkRepeated(divisor, dividend) {
    const divisorMagnitude = divisor < 0n ? -divisor : divisor;
    for (let i = 0; i < 150; i++) {
        const actual = dividend % divisor;
        shouldBe(actual, refMod(dividend, divisor));
        // BigInt % is truncated, so the result takes the sign of the dividend.
        const magnitude = actual < 0n ? -actual : actual;
        if (magnitude >= divisorMagnitude)
            throw new Error(`${actual} out of range for divisor ${divisor}`);
    }
}

// Sweep dividends of n through 2n digits, both signs, for one divisor.
function checkAllDividendSizes(divisor, width, n) {
    for (let size = n; size <= 2 * n; size++) {
        const top = 1n << BigInt(size * width);
        for (const dividend of [top - 1n, top - 12345n, top >> 1n, (top >> 1n) | 1n]) {
            checkRepeated(divisor, dividend);
            checkRepeated(divisor, -dividend);
        }
    }
}

// Qualifying divisors: 2^k - c with a small c, so the reduction factor stays a single digit.
for (const width of digitWidths) {
    for (const n of [2, 3, 4, 5, 6, 8]) {
        const bits = n * width;
        for (const c of [1n, 19n, 977n, 4294968273n]) {
            const divisor = (1n << BigInt(bits)) - c;
            if (divisor <= 0n)
                continue;
            checkAllDividendSizes(divisor, width, n);
        }
        // Not filling the top digit still qualifies: the ed25519 prime is 2^255 - 19 on a 4-digit
        // 64-bit layout, so the factor is 2 * 19 rather than 19.
        const shortDivisor = (1n << BigInt(bits - 1)) - 19n;
        if (shortDivisor > 0n)
            checkAllDividendSizes(shortDivisor, width, n);
    }
}

// The real curve primes this path exists for.
const ed25519P = (1n << 255n) - 19n;
const secp256k1P = (1n << 256n) - (1n << 32n) - 977n;
const mersenne127 = (1n << 127n) - 1n;
const mersenne521 = (1n << 521n) - 1n;
for (const p of [ed25519P, secp256k1P, mersenne127, mersenne521]) {
    checkRepeated(p, p - 1n);
    checkRepeated(p, p);
    checkRepeated(p, p + 1n);
    checkRepeated(p, p * 2n - 1n);
    checkRepeated(p, p * p - 1n);
    let x = 123456789n;
    for (let i = 0; i < 200; i++) {
        // Reduce the unreduced square, not the already-reduced result: comparing x against
        // refMod(x, p) would be comparing x against itself, since x is below p by construction.
        const square = x * x;
        x = square % p;
        if (x >= p)
            throw new Error(`squaring escaped the modulus: ${x}`);
        shouldBe(x, refMod(square, p));
    }
}

// The quotient cap is maxFoldQuotient = 4, and q alone sets how many times the corrective loop
// subtracts, so cover the q = 3 and q = 4 qualifying boundary as well as q = 5, which must not
// qualify and falls back to the inverse path. Each divisor is built from T = q * B + C with a
// single-digit C:
//   2^256 = 3 * ((2^256 - 4) / 3) + 4            (q = 3)
//   2^256 = 4 * (2^254 - 1) + 4                  (q = 4, the largest admitted quotient)
//   2^128 = 4 * (2^126 - 2^62 + 1) + (2^64 - 4)  (q = 4 with C near the top of a 64-bit digit;
//                                                on 32-bit targets C spans two digits, so this
//                                                exercises the inverse path there instead)
//   2^256 = 5 * ((2^256 - 1) / 5) + 1            (q = 5, rejected by the cap)
for (const width of digitWidths) {
    const n256 = 256 / width;
    const n128 = 128 / width;
    checkAllDividendSizes(((1n << 256n) - 4n) / 3n, width, n256);
    checkAllDividendSizes((1n << 254n) - 1n, width, n256);
    checkAllDividendSizes((1n << 126n) - (1n << 62n) + 1n, width, n128);
    checkAllDividendSizes(((1n << 256n) - 1n) / 5n, width, n256);
}

// Negative divisors take the same cached paths; only the result sign differs.
for (const p of [ed25519P, secp256k1P]) {
    checkRepeated(-p, p * p - 1n);
    checkRepeated(-p, -(p * 2n + 12345n));
}

// Non-qualifying divisors must keep using the multiplicative inverse and still be correct. These
// are the standard curve moduli whose reduction factor spans more than one digit.
const nonQualifying = [
    // ed25519 group order.
    (1n << 252n) + 27742317777372353535851937790883648493n,
    // secp256k1 group order.
    0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141n,
    // NIST P-256 and P-384.
    (1n << 256n) - (1n << 224n) + (1n << 192n) + (1n << 96n) - 1n,
    (1n << 384n) - (1n << 128n) - (1n << 96n) + (1n << 32n) - 1n,
    // bls12-381 base field.
    0x1a0111ea397fe69a4b1ba7b6434bacd764774b84f38512bf6730d2a0f6b0f6241eabfffeb153ffffb9feffffffffaaabn,
];
for (const m of nonQualifying) {
    checkRepeated(m, m - 1n);
    checkRepeated(m, m * m - 1n);
    checkRepeated(m, (m << 1n) + 12345n);
    let x = 987654321n;
    for (let i = 0; i < 200; i++) {
        const square = x * x;
        x = square % m;
        if (x >= m)
            throw new Error(`squaring escaped the modulus: ${x}`);
        shouldBe(x, refMod(square, m));
    }
}

// Switching between divisors of the same digit count: the reduction factor belongs to whichever
// divisor is armed, so a stale one must never be applied. Arming takes 100 consecutive uses of one
// divisor, and while neither divisor is armed a strictly alternating loop puts each call in the
// third branch of the cache and resets the counter, so nothing is ever armed and none of this code
// runs. Each divisor therefore gets a run of its own past the threshold first; the run right after
// a switch is what would expose a factor or inverse left behind by the previous divisor.
function runOfOneDivisor(divisor, dividendAt) {
    for (let i = 0; i < 150; i++) {
        const dividend = dividendAt(i);
        shouldBe(dividend % divisor, refMod(dividend, divisor));
    }
}

// A qualifying divisor against a non-qualifying one of the same size, so arming has to swap between
// the fold factor and the multiplicative inverse in both directions.
{
    const qualifying = ed25519P;
    const other = (1n << 256n) - (1n << 224n) + (1n << 192n) + (1n << 96n) - 1n;
    const dividendAt = i => (1n << 500n) - BigInt(i) * 7919n;
    runOfOneDivisor(qualifying, dividendAt);
    runOfOneDivisor(other, dividendAt);
    runOfOneDivisor(qualifying, dividendAt);
    for (let i = 0; i < 400; i++) {
        const dividend = dividendAt(i);
        shouldBe(dividend % qualifying, refMod(dividend, qualifying));
        shouldBe(dividend % other, refMod(dividend, other));
    }
}

// Two qualifying divisors of the same size, so each must use its own factor.
{
    const a = ed25519P;
    const b = secp256k1P;
    const dividendAt = i => (1n << 480n) + BigInt(i) * 104729n;
    runOfOneDivisor(a, dividendAt);
    runOfOneDivisor(b, dividendAt);
    runOfOneDivisor(a, dividendAt);
    for (let i = 0; i < 400; i++) {
        const dividend = dividendAt(i);
        shouldBe(dividend % a, refMod(dividend, a));
        shouldBe(dividend % b, refMod(dividend, b));
    }
}

// Divisors of more than maxInPlaceCachedModSize digits (8 at a 64-bit digit width) return their
// remainder through the out-of-line result path. That is the only fold path where an all-zero
// result has to normalize to 0n, and the only one where a negative dividend could otherwise
// produce a negative zero.
for (const bits of [640n, 1280n]) {
    const divisor = (1n << bits) - 19n;
    checkAllDividendSizes(divisor, 64, Number(bits) / 64);
    checkRepeated(divisor, divisor);
    checkRepeated(divisor, -divisor);
    checkRepeated(divisor, divisor * 3n);
    checkRepeated(divisor, -(divisor * 3n));
}

// Powers of two have a zero reduction factor, which the factor check rejects, so they keep using
// the multiplicative inverse; make sure they stay correct.
for (const bits of [128n, 192n, 256n, 320n]) {
    const divisor = 1n << bits;
    checkRepeated(divisor, divisor - 1n);
    checkRepeated(divisor, divisor * divisor - 1n);
    checkRepeated(divisor, (divisor << 1n) | 1n);
}
