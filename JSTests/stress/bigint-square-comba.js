// Squaring a BigInt by itself takes a dedicated Comba path that accumulates each off-diagonal
// product once at twice the weight. Its result must match the general multiply for every operand
// width, including the widths that have a fully unrolled kernel (1, 2, 4, 8 and 16 digits).

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

// A distinct BigInt cell holding the same value, so that x * copy(x) cannot take the squaring path.
function copy(x) {
    return BigInt(x.toString());
}

function checkSquare(x, label) {
    let expected = x * copy(x);
    shouldBe(x * x, expected, `${label}: x * x`);
    shouldBe((-x) * (-x), expected, `${label}: (-x) * (-x)`);
    shouldBe(x * (-x), -expected, `${label}: x * (-x)`);
    shouldBe((-x) * x, -expected, `${label}: (-x) * x`);
    // Independent of the multiply path: x^2 == x * (x - 1) + x.
    shouldBe(expected, x * copy(x - 1n) + x, `${label}: x * (x - 1) + x`);
    shouldBe(x ** 2n, expected, `${label}: x ** 2n`);
}

let seed = 0x12345678n;
function nextDigit() {
    // 64-bit LCG, so the test data is fixed but exercises every digit position.
    seed = (seed * 6364136223846793005n + 1442695040888963407n) & 0xffffffffffffffffn;
    return seed;
}

function fromDigits(digits) {
    let result = 0n;
    for (let i = digits.length - 1; i >= 0; --i)
        result = (result << 64n) | digits[i];
    return result;
}

// Widths 1..17 digits cover both the unrolled kernels and the size-agnostic fallbacks, plus the
// boundaries just above each of them.
for (let length = 1; length <= 17; ++length) {
    let allOnes = (1n << BigInt(64 * length)) - 1n;
    checkSquare(allOnes, `all ones, ${length} digits`);

    checkSquare(1n << BigInt(64 * length - 1), `top bit only, ${length} digits`);
    checkSquare((1n << BigInt(64 * (length - 1))) + 1n, `lowest and highest digit, ${length} digits`);

    // Small top digit: the product is one digit shorter than length * 2, so the caller has to trim.
    let digits = [];
    for (let i = 0; i < length; ++i)
        digits.push(nextDigit());
    digits[length - 1] = 1n;
    checkSquare(fromDigits(digits), `small top digit, ${length} digits`);

    // Zero digits in the middle must not disturb the carry chain.
    digits = [];
    for (let i = 0; i < length; ++i)
        digits.push(i % 2 ? 0n : nextDigit() | 1n);
    checkSquare(fromDigits(digits), `alternating zero digits, ${length} digits`);

    for (let round = 0; round < 4; ++round) {
        digits = [];
        for (let i = 0; i < length; ++i)
            digits.push(nextDigit());
        digits[length - 1] |= 1n << 63n;
        checkSquare(fromDigits(digits), `random, ${length} digits, round ${round}`);
    }
}

checkSquare(0n, "zero");
checkSquare(1n, "one");
checkSquare(0xffffffffffffffffn, "one all-ones digit");

// (a + b)^2 == a^2 + 2ab + b^2 ties the squaring path back to addition and the general multiply.
for (let length = 1; length <= 9; ++length) {
    let a = fromDigits(Array.from({ length }, () => nextDigit()));
    let b = fromDigits(Array.from({ length }, () => nextDigit()));
    let sum = a + b;
    shouldBe(sum * sum, a * a + 2n * (a * copy(b)) + b * b, `binomial, ${length} digits`);
}

// Modular exponentiation squares the same cell repeatedly, which is the shape this path exists for.
{
    let base = 0xdeadbeefcafebabe0123456789abcdefn;
    let modulus = (1n << 255n) - 19n;
    let expected = 1n;
    for (let i = 0; i < 64; ++i)
        expected = (expected * copy(base)) % modulus;
    let actual = 1n;
    let running = base;
    let exponent = 64n;
    // Square-and-multiply, so every step squares one cell.
    while (exponent > 0n) {
        if (exponent & 1n)
            actual = (actual * running) % modulus;
        running = (running * running) % modulus;
        exponent >>= 1n;
    }
    shouldBe(actual, expected, "modular exponentiation");
}
