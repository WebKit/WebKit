//@ slow!
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

function refMul(a, b) {
    let result = 0n;
    let shift = 0n;
    while (b > 0n) {
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
    return value | (1n << BigInt(width * digits - 1));
}

function makeSparseOperand(digits, width, seed) {
    let value = 1n << BigInt(width * digits - 1);
    for (let i = 0; i < digits; i++) {
        if ((i * 7 + seed) % 5 === 0)
            value |= ((1n << BigInt(width)) - 1n) << BigInt(width * i);
    }
    return value;
}

function check(x, y) {
    const expected = refMul(x, y);
    shouldBe(x * y, expected);
    shouldBe(y * x, expected);
    shouldBe((-x) * y, -expected);
    shouldBe((-x) * (-y), expected);
}

for (const width of [32, 64]) {
    for (const size of [33, 34, 35, 36, 37, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 64, 67, 68, 69, 70, 72, 79, 80, 81, 87, 88, 89, 96, 97, 100, 104, 136, 137, 144, 200, 255, 256, 257, 300, 512, 513]) {
        const x = makeOperand(size, width, size);
        check(x, makeOperand(size, width, size * 3 + 1));
        shouldBe(x * x, refMul(x, x));
    }

    for (const [larger, smaller] of [[35, 34], [40, 34], [40, 39], [41, 40], [44, 43], [45, 44], [68, 34], [69, 34], [80, 40], [81, 40], [88, 44], [89, 44], [100, 34], [1000, 34], [1000, 39], [1000, 43], [70, 35], [97, 36], [104, 97], [200, 97], [300, 128], [1000, 40], [1000, 41], [1000, 44], [1000, 45], [1000, 100], [1000, 257], [2100, 70]]) {
        check(makeOperand(larger, width, larger + smaller), makeOperand(smaller, width, larger * smaller));
    }

    for (const size of [34, 36, 40, 41, 44, 45, 68, 80, 88, 97, 104, 256, 257]) {
        const ones = (1n << BigInt(width * size)) - 1n;
        check(ones, ones);
        check(ones, ones - (1n << BigInt(width * (size - 1))));
        check(ones << BigInt(width * size), ones);
        check(1n << BigInt(width * size), (1n << BigInt(width * size)) + 1n);
    }

    for (const size of [36, 40, 44, 70, 80, 88, 104, 136, 257]) {
        const sparse = makeSparseOperand(size, width, size);
        check(sparse, makeSparseOperand(size, width, size + 1));
        check(sparse, makeOperand(size, width, size * 2));
        check(makeOperand(size * 3, width, size), sparse);
    }
}
