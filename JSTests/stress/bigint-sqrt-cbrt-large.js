//@ requireOptions("--useBigIntMathMethods=1")

// BigInt.sqrt and BigInt.cbrt divide into a quotient buffer longer than the quotient, and their first
// divisor is a power of two. Operands this long take the Burnikel-Ziegler division.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected.toString(16).slice(0, 40)}... but got ${actual.toString(16).slice(0, 40)}...`);
}

function shouldBeRoot(root, power, x, message) {
    if (root ** power > x || (root + 1n) ** power <= x)
        throw new Error(`${message}: got ${root.toString(16).slice(0, 40)}...`);
}

function makeOperand(width, digits, seed) {
    const hexDigits = width / 4;
    const shift = BigInt(64 - width);
    const parts = new Array(digits);
    let mix = BigInt.asUintN(64, 0x9e3779b97f4a7c15n * BigInt(seed + 1));
    for (let i = 0; i < digits; i++) {
        mix = BigInt.asUintN(64, mix * 6364136223846793005n + 1442695040888963407n);
        parts[i] = (mix >> shift).toString(16).padStart(hexDigits, "0");
    }
    parts[0] = "8" + parts[0].slice(1);
    return BigInt("0x" + parts.join(""));
}

for (const width of [32, 64]) {
    for (const digits of [200, 301, 1000]) {
        const x = makeOperand(width, digits, digits);
        shouldBeRoot(BigInt.sqrt(x), 2n, x, `${width} bit digits, sqrt of ${digits}`);
        shouldBeRoot(BigInt.cbrt(x), 3n, x, `${width} bit digits, cbrt of ${digits}`);

        const root = makeOperand(width, digits >> 2, digits + 1);
        const square = root * root;
        shouldBe(BigInt.sqrt(square), root, `${width} bit digits, sqrt of a ${digits >> 1} digit square`);
        shouldBe(BigInt.sqrt(square - 1n), root - 1n, `${width} bit digits, sqrt below a ${digits >> 1} digit square`);
        const cube = square * root;
        shouldBe(BigInt.cbrt(cube), root, `${width} bit digits, cbrt of a cube of ${digits >> 2} digits`);
        shouldBe(BigInt.cbrt(cube - 1n), root - 1n, `${width} bit digits, cbrt below a cube of ${digits >> 2} digits`);
    }
}
