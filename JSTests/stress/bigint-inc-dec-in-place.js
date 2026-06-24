function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

function inc(n) { return ++n; }
function dec(n) { return --n; }
noInline(inc);
noInline(dec);

const oneDigitBits = 64n;
function pow2(n) { return 1n << n; }

// Zero crossing.
shouldBe(inc(0n), 1n);
shouldBe(dec(0n), -1n);
shouldBe(inc(-1n), 0n);
shouldBe(dec(1n), 0n);
shouldBe(inc(-2n), -1n);
shouldBe(dec(-1n), -2n);

// BigInt32 boundaries (under USE(BIGINT32) these transition to/from heap BigInt).
shouldBe(inc(2147483647n), 2147483648n);
shouldBe(dec(2147483648n), 2147483647n);
shouldBe(inc(-2147483649n), -2147483648n);
shouldBe(dec(-2147483648n), -2147483649n);

// Carry propagation across digit boundaries: length grows by one digit.
for (const L of [1, 2, 3, 4, 16, 17, 64]) {
    const allOnes = pow2(BigInt(L) * oneDigitBits) - 1n; // L digits of all-ones
    shouldBe(inc(allOnes), pow2(BigInt(L) * oneDigitBits));
    shouldBe(dec(-allOnes), -pow2(BigInt(L) * oneDigitBits));
}

// Borrow propagation: length shrinks by one digit.
for (const L of [2, 3, 4, 16, 17, 64]) {
    const powerOfDigit = pow2(BigInt(L - 1) * oneDigitBits); // L digits: [0, ..., 0, 1]
    shouldBe(dec(powerOfDigit), powerOfDigit - 1n);
    shouldBe(inc(-powerOfDigit), -(powerOfDigit - 1n));
}

// No carry/borrow out of the top digit: length stays the same.
for (const L of [1, 2, 3, 16, 64]) {
    const x = pow2(BigInt(L) * oneDigitBits - 1n) | 12345n;
    shouldBe(inc(x), x + 1n);
    shouldBe(dec(x), x - 1n);
    shouldBe(inc(-x), -(x - 1n));
    shouldBe(dec(-x), -(x + 1n));
}

// Carry stops in the middle: low digit all-ones, higher digits not.
for (const L of [2, 3, 16]) {
    const x = (pow2(BigInt(L) * oneDigitBits - 1n)) | (pow2(oneDigitBits) - 1n);
    shouldBe(inc(x), x + 1n);
    shouldBe(dec(x + 1n), x);
    shouldBe(dec(-x), -(x + 1n));
    shouldBe(inc(-(x + 1n)), -x);
}

// Repeated inc/dec walking across a digit boundary, cross-checked against +/-.
{
    let n = pow2(oneDigitBits) - 5n;
    let m = n;
    for (let i = 0; i < 10; i++) {
        n = inc(n);
        m = m + 1n;
        shouldBe(n, m);
    }
    for (let i = 0; i < 20; i++) {
        n = dec(n);
        m = m - 1n;
        shouldBe(n, m);
    }
    shouldBe(n, pow2(oneDigitBits) - 15n);
}

// Operands must not be mutated (results are fresh cells).
{
    const x = pow2(130n) - 1n;
    const before = x.toString();
    inc(x);
    dec(x);
    shouldBe(x.toString(), before);
}

// maxLength boundary: maxLengthBits = 1 << 20.
{
    const maxLengthBits = 1048576n;
    // 2^maxLengthBits - 1 (exactly maxLength digits, all-ones), built without
    // materializing 2^maxLengthBits itself.
    const max = ((pow2(maxLengthBits - 1n) - 1n) << 1n) | 1n;
    shouldBe(dec(max), max - 1n);
    shouldBe(inc(max - 1n), max);
    shouldBe(inc(-max), -(max - 1n));
    let caught = null;
    try {
        inc(max); // would need maxLength + 1 digits
    } catch (error) {
        caught = error;
    }
    shouldBe(caught instanceof RangeError, true);
    caught = null;
    try {
        dec(-max);
    } catch (error) {
        caught = error;
    }
    shouldBe(caught instanceof RangeError, true);
    // No overflow when the carry does not propagate to a new digit.
    const nearMax = max - pow2(maxLengthBits - 1n); // still maxLength digits
    shouldBe(inc(nearMax), nearMax + 1n);
}

// Stress with tiering: keep inc/dec hot on multi-digit values.
{
    const base = pow2(192n) - 2n;
    let n = base;
    for (let i = 0; i < 1e4; i++)
        n = inc(n);
    shouldBe(n, base + 10000n);
    for (let i = 0; i < 1e4; i++)
        n = dec(n);
    shouldBe(n, base);
}
