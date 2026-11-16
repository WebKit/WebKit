function shouldBe(actual, expected, message) {
    // Distinguish -0 from 0 and treat NaN as equal to NaN.
    let same = actual === expected ? (actual !== 0 || 1 / actual === 1 / expected)
        : (actual !== actual && expected !== expected);
    if (!same)
        throw new Error(`${message}: expected ${expected === 0 && 1 / expected < 0 ? "-0" : expected}, got ${actual === 0 && 1 / actual < 0 ? "-0" : actual}`);
}

function negLeft(w, r) { return -w * r; }
noInline(negLeft);

function negRight(w, r) { return w * -r; }
noInline(negRight);

function negBoth(w, r) { return -w * -r; }
noInline(negBoth);

// The negation has two users, so it cannot be absorbed into the multiply.
function sharedNeg(w, r) {
    let n = -w;
    return n * r + n;
}
noInline(sharedNeg);

// Integer operands, so this stays on the Int32 path.
function negLeftInt(w, r) { return (-w * r) | 0; }
noInline(negLeftInt);

let signedZeroCases = [
    // w, r, -w * r, w * -r, -w * -r
    [0, 3, -0, -0, 0],
    [-0, 3, 0, 0, -0],
    [0, -3, 0, 0, -0],
    [-0, -3, -0, -0, 0],
    [0, 0, -0, -0, 0],
    [-0, -0, -0, -0, 0],
    [0, -0, 0, 0, -0],
];

let specialCases = [
    // w, r, -w * r, w * -r, -w * -r
    [0, Infinity, NaN, NaN, NaN],
    [2, Infinity, -Infinity, -Infinity, Infinity],
    [-2, Infinity, Infinity, Infinity, -Infinity],
    [NaN, 3, NaN, NaN, NaN],
    [3, NaN, NaN, NaN, NaN],
    [1.5, 2.5, -3.75, -3.75, 3.75],
    [-1.5, 2.5, 3.75, 3.75, -3.75],
    [Number.MIN_VALUE, 0.5, -Number.MIN_VALUE / 2, -Number.MIN_VALUE / 2, Number.MIN_VALUE / 2],
    [Number.MAX_VALUE, 2, -Infinity, -Infinity, Infinity],
];

let cases = signedZeroCases.concat(specialCases);

for (let i = 0; i < 1e4; ++i) {
    for (let [w, r, expectNegLeft, expectNegRight, expectNegBoth] of cases) {
        shouldBe(negLeft(w, r), expectNegLeft, `-${w} * ${r}`);
        shouldBe(negRight(w, r), expectNegRight, `${w} * -${r}`);
        shouldBe(negBoth(w, r), expectNegBoth, `-${w} * -${r}`);
        shouldBe(sharedNeg(w, r), expectNegLeft + -w, `shared -${w} * ${r}`);
    }
}

// Int32 path: (-w * r) | 0 wraps like the two's complement product.
let intCases = [
    [1, 1, -1],
    [-1, 1, 1],
    [3, 7, -21],
    [-3, 7, 21],
    [0x7fffffff, 2, 2],
    [-0x80000000, 2, 0],
    [0x10000, 0x10000, 0],
];

for (let i = 0; i < 1e4; ++i) {
    for (let [w, r, expected] of intCases)
        shouldBe(negLeftInt(w, r), expected, `(-${w} * ${r}) | 0`);
}
