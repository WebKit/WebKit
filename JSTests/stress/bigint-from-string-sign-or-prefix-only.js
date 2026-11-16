function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${String(expected)} but got ${String(actual)}`);
}

function shouldThrowSyntaxError(func, message) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`${message}: expected SyntaxError but got ${String(error)}`);
}

const invalid = [
    "-", "+", " - ", " + ", "-\n", "+\t", "\n-", "\t+ \n",
    "- ", "+　", "﻿-﻿",
    "0x ", "0X\t", " 0b\t", "0B\n", "0o\n", " 0O ", "0x ", "0b ", "0o　",
    "0x", "0b", "0o", "- 1", "+ 1", "+-1", "-+1", "--1", "-0x1", "+0b1", "0x 1", "0x-1",
];

const valid = [
    ["", 0n], [" ", 0n], [" \n\t ", 0n], ["　", 0n],
    ["0", 0n], ["-0", 0n], ["+0", 0n], [" -0 ", 0n], ["000", 0n], ["-000 ", 0n],
    ["0x0 ", 0n], [" 0b0\n", 0n], ["0o00\t", 0n], ["0x0　", 0n],
    ["1 ", 1n], ["+1 ", 1n], [" -12 ", -12n], ["-12 ", -12n],
    ["0x10 ", 16n], [" 0b101\n", 5n], ["0o17\t", 15n],
    ["-123456789012345678901234567890 ", -123456789012345678901234567890n],
];

function looseEqual(a, b) { return a == b; }
function greaterOrEqual(a, b) { return a >= b; }
function less(a, b) { return a < b; }
noInline(looseEqual);
noInline(greaterOrEqual);
noInline(less);

const big = 1n << 64n;
const int64Array = new BigInt64Array(1);
const dataView = new DataView(new ArrayBuffer(8));

for (let i = 0; i < testLoopCount; ++i) {
    const string = invalid[i % invalid.length];
    const message = JSON.stringify(string);

    shouldBe(looseEqual(0n, string), false, `0n == ${message}`);
    shouldBe(looseEqual(string, 0n), false, `${message} == 0n`);
    shouldBe(looseEqual(big, string), false, `big == ${message}`);
    shouldBe(greaterOrEqual(string, 0n), false, `${message} >= 0n`);
    shouldBe(greaterOrEqual(0n, string), false, `0n >= ${message}`);
    shouldBe(less(string, 1n), false, `${message} < 1n`);
    shouldBe(less(string, big), false, `${message} < big`);
    shouldBe(less(-big, string), false, `-big < ${message}`);
}

for (const string of invalid) {
    const message = JSON.stringify(string);
    shouldThrowSyntaxError(() => BigInt(string), `BigInt(${message})`);
    shouldThrowSyntaxError(() => BigInt.asIntN(8, string), `BigInt.asIntN(8, ${message})`);
    shouldThrowSyntaxError(() => BigInt.asUintN(8, string), `BigInt.asUintN(8, ${message})`);
    shouldThrowSyntaxError(() => { int64Array[0] = string; }, `BigInt64Array store of ${message}`);
    shouldThrowSyntaxError(() => dataView.setBigInt64(0, string), `DataView#setBigInt64 of ${message}`);
}

for (const [string, expected] of valid) {
    const message = JSON.stringify(string);
    shouldBe(BigInt(string), expected, `BigInt(${message})`);
    shouldBe(looseEqual(expected, string), true, `${expected}n == ${message}`);
    shouldBe(greaterOrEqual(string, expected), true, `${message} >= ${expected}n`);
    shouldBe(less(string, expected), false, `${message} < ${expected}n`);
    int64Array[0] = string;
    shouldBe(int64Array[0], BigInt.asIntN(64, expected), `BigInt64Array store of ${message}`);
}
