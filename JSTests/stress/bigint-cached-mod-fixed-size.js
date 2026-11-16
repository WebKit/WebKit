function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

// cachedMod dispatches to a size-specialized implementation for divisors of certain digit
// counts. Cover every dividend size that path accepts (n to 2n digits) for those divisors,
// plus the sizes that must still fall back to the generic implementation.
//
// The cache is only armed after 100 uses of the same divisor, so each case loops past that
// threshold to make sure both the pre-cache and post-cache paths produce the same answer.
//
// A Digit is a CPU register, so it is 64 bits on 64-bit targets and 32 bits on 32-bit ones, and a
// given bit width lands on a different specialization on each. Every size-driven block below runs
// at both widths so the same set of digit counts is covered either way.
const digitWidths = [32, 64];

// Reference modulo that does not go through the cached path.
function refMod(a, b) {
    return a - (a / b) * b;
}

function checkRepeated(divisor, dividend) {
    for (let i = 0; i < 150; i++) {
        const actual = dividend % divisor;
        shouldBe(actual, refMod(dividend, divisor));
        // BigInt % is truncated, so the result takes the sign of the dividend.
        const magnitude = actual < 0n ? -actual : actual;
        if (magnitude >= divisor)
            throw new Error(`${actual} out of range for divisor ${divisor}`);
    }
}

// A divisor of n digits with dividends of n, n+1, ... 2n digits.
for (const width of digitWidths) {
    for (const n of [2, 3, 4, 5, 6, 8]) {
        const bits = width * n;
        const divisor = (1n << BigInt(bits)) - 159n;
        for (let extra = 0; extra <= n; extra++) {
            // Dividend with exactly n + extra digits.
            let dividend = (1n << BigInt(bits + width * extra - 1)) | 0x9e3779b97f4a7c15n;
            checkRepeated(divisor, dividend);
            checkRepeated(divisor, -dividend);
        }
    }
}

// Dividend equal to and just below/above the divisor, where the corrective loop is tightest.
// This is also the only block that produces a dividend of exactly the divisor's width, so every
// divisor size the specialized path accepts has to appear here.
for (const width of digitWidths) {
    for (const n of [2, 3, 4, 6]) {
        const divisor = (1n << BigInt(width * n)) - 189n;
        for (const dividend of [divisor - 1n, divisor, divisor + 1n, divisor * 2n - 1n, divisor * divisor - 1n]) {
            checkRepeated(divisor, dividend);
            checkRepeated(divisor, -dividend);
        }
    }
}

// Divisors whose top digit is small, so the inverse has a leading zero digit.
for (const width of digitWidths) {
    for (const n of [2, 4]) {
        const divisor = (1n << BigInt(width * (n - 1) + 1)) + 1n;
        checkRepeated(divisor, divisor * divisor - 3n);
        checkRepeated(divisor, (divisor << BigInt(width)) + 7n);
    }
}

// All-ones divisor exercises the overflow guard in the inverse computation.
for (const width of digitWidths) {
    for (const n of [2, 4]) {
        const divisor = (1n << BigInt(width * n)) - 1n;
        checkRepeated(divisor, divisor * divisor - 1n);
        checkRepeated(divisor, divisor + 1n);
    }
}

// ed25519 and secp256k1 moduli with known-good results, the shapes real curve code produces.
{
    const p = 2n ** 255n - 19n;
    let x = 12345678901234567890123456789n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 36260996769751896565040950513605801167497797310772705034288993169759633740077n);
}
{
    const p = 2n ** 256n - 2n ** 32n - 977n;
    let x = 98765432109876543210987654321n;
    for (let i = 0; i < 200; i++)
        x = (x * x) % p;
    shouldBe(x, 48856533044910686660523345883058546538386844742543846581192521152919987323764n);
}

// Interleaving two divisors of the same size makes sure a cached inverse is never applied to the
// wrong divisor. The cache only arms after 100 consecutive uses of one divisor, so `a` has to be
// armed on its own before interleaving reaches the case worth testing.
{
    const a = (1n << 256n) - 189n;
    const b = (1n << 256n) - 357n;
    const dividends = [];
    for (let i = 0; i < 16; i++)
        dividends.push((1n << 400n) + BigInt(i) * 0x9e3779b97f4a7c15n + 12345n);

    for (let i = 0; i < 150; i++) {
        const x = dividends[i & 15];
        shouldBe(x % a, refMod(x, a));
    }
    for (let i = 0; i < 150; i++) {
        const x = dividends[i & 15];
        shouldBe(x % a, refMod(x, a));
        shouldBe(x % b, refMod(x, b));
    }
}
