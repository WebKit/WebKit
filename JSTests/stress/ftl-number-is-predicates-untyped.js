function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

function isNaNTest(value) { return Number.isNaN(value); }
function isFiniteTest(value) { return Number.isFinite(value); }
function isSafeIntegerTest(value) { return Number.isSafeInteger(value); }
noInline(isNaNTest);
noInline(isFiniteTest);
noInline(isSafeIntegerTest);

let values = [
    0, -0, 1, -1, 0x7fffffff, -0x80000000, 1.5, -1.5, NaN, Infinity, -Infinity,
    2 ** 53 - 1, -(2 ** 53 - 1), 2 ** 53, -(2 ** 53), 1e300, Number.MIN_VALUE,
    "1", "NaN", "", null, undefined, true, false, {}, [], Symbol(), 1n, 0n, () => 1,
];

for (let i = 0; i < testLoopCount; ++i) {
    let value = values[i % values.length];
    shouldBe(isNaNTest(value), typeof value === "number" && value !== value, `Number.isNaN(${String(value)})`);
    shouldBe(isFiniteTest(value), typeof value === "number" && value === value && value !== Infinity && value !== -Infinity, `Number.isFinite(${String(value)})`);
    shouldBe(isSafeIntegerTest(value), typeof value === "number" && Number.isInteger(value) && Math.abs(value) <= 2 ** 53 - 1, `Number.isSafeInteger(${String(value)})`);
}
