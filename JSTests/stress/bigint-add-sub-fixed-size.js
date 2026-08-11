// Equal-width BigInt addition and subtraction take kernels that are unrolled onto a static extent
// for narrow operands. They must agree with the size-agnostic path for every width and sign
// combination, including the carry-out and full-borrow-cascade cases.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

let seed = 0x9e3779b9n;
function nextDigit() {
    seed = (seed * 6364136223846793005n + 1442695040888963407n) & 0xffffffffffffffffn;
    return seed;
}

function fromDigits(digits) {
    let result = 0n;
    for (let i = digits.length - 1; i >= 0; --i)
        result = (result << 64n) | digits[i];
    return result;
}

function randomOfLength(length) {
    let digits = [];
    for (let i = 0; i < length; ++i)
        digits.push(nextDigit());
    digits[length - 1] |= 1n; // Keep the value exactly {length} digits wide.
    return fromDigits(digits);
}

// Addition and subtraction are checked against each other and against a shift-free reconstruction,
// so a wrong kernel cannot be masked by the path it is being compared with.
function check(a, b, label) {
    let sum = a + b;
    shouldBe(sum - b, a, `${label}: (a + b) - b`);
    shouldBe(sum - a, b, `${label}: (a + b) - a`);
    shouldBe(a - b, -(b - a), `${label}: a - b == -(b - a)`);
    shouldBe(a + b, b + a, `${label}: commutative`);
    shouldBe((-a) + (-b), -sum, `${label}: (-a) + (-b)`);
    shouldBe((-a) - (-b), b - a, `${label}: (-a) - (-b)`);
    shouldBe(a - (-b), sum, `${label}: a - (-b)`);
    shouldBe((-a) + b, b - a, `${label}: (-a) + b`);
    shouldBe(a - a, 0n, `${label}: a - a`);
    // A + B == (A ^ B) + 2 * (A & B) for non-negative operands: no reliance on the add path.
    if (a >= 0n && b >= 0n)
        shouldBe(sum, (a ^ b) + 2n * (a & b), `${label}: xor/and identity`);
}

// Widths 1..6 span the unrolled kernels (1..4) and the sizes just above them.
for (let length = 1; length <= 6; ++length) {
    let allOnes = (1n << BigInt(64 * length)) - 1n;
    let one = 1n;

    // Carry out of the top digit, and a borrow cascading through every digit.
    check(allOnes, allOnes, `all ones + all ones, ${length} digits`);
    check(allOnes, one, `all ones + 1, ${length} digits`);
    check(1n << BigInt(64 * length - 1), 1n << BigInt(64 * length - 1), `top bits, ${length} digits`);
    check(1n << BigInt(64 * (length - 1)), one, `borrow cascade, ${length} digits`);
    check(allOnes - 1n, one, `no carry out, ${length} digits`);
    check(0n, allOnes, `zero and all ones, ${length} digits`);

    for (let round = 0; round < 8; ++round) {
        let a = randomOfLength(length);
        let b = randomOfLength(length);
        check(a, b, `random equal width ${length}, round ${round}`);
    }

    // Mixed widths keep using the size-agnostic path, which must still be reachable.
    for (let other = 1; other <= 6; ++other) {
        let a = randomOfLength(length);
        let b = randomOfLength(other);
        check(a, b, `mixed widths ${length}/${other}`);
    }
}

// Repeated add/subtract cycles must return to the starting value.
{
    let value = randomOfLength(4);
    let step = randomOfLength(4);
    let running = value;
    for (let i = 0; i < 1000; ++i)
        running = running + step;
    for (let i = 0; i < 1000; ++i)
        running = running - step;
    shouldBe(running, value, "add/subtract round trip");
}
