function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const Greater = 0;
const Less = 1;
const Equal = 2;
const Undefined = 3;

function result(func, data)
{
    if (data === Undefined)
        return false;
    if (func === lessThan)
        return data === Less;
    if (func === lessThanEqual)
        return data === Less || data === Equal;
    if (func === greaterThan)
        return data === Greater;
    if (func === greaterThanEqual)
        return data === Greater || data === Equal;
    if (func === equal)
        return data === Equal;
    return false;
}

var list = [
    [createHeapBigInt(1n), 2, Less],
    [createHeapBigInt(1n), 1, Equal],
    [createHeapBigInt(1n), 0, Greater],
    [createHeapBigInt(1n), -1, Greater],
    [createHeapBigInt(1n), -2, Greater],
    [createHeapBigInt(0n), 2, Less],
    [createHeapBigInt(0n), 1, Less],
    [createHeapBigInt(0n), 0, Equal],
    [createHeapBigInt(0n), -1, Greater],
    [createHeapBigInt(0n), -2, Greater],
    [createHeapBigInt(-1n), 2, Less],
    [createHeapBigInt(-1n), 1, Less],
    [createHeapBigInt(-1n), 0, Less],
    [createHeapBigInt(-1n), -1, Equal],
    [createHeapBigInt(-1n), -2, Greater],

    [1, createHeapBigInt(2n), Less],
    [1, createHeapBigInt(1n), Equal],
    [1, createHeapBigInt(0n), Greater],
    [1, createHeapBigInt(-1n), Greater],
    [1, createHeapBigInt(-2n), Greater],
    [0, createHeapBigInt(2n), Less],
    [0, createHeapBigInt(1n), Less],
    [0, createHeapBigInt(0n), Equal],
    [0, createHeapBigInt(-1n), Greater],
    [0, createHeapBigInt(-2n), Greater],
    [-1, createHeapBigInt(2n), Less],
    [-1, createHeapBigInt(1n), Less],
    [-1, createHeapBigInt(0n), Less],
    [-1, createHeapBigInt(-1n), Equal],
    [-1, createHeapBigInt(-2n), Greater],

    [1, 2n, Less],
    [1, 1n, Equal],
    [1, 0n, Greater],
    [1, -1n, Greater],
    [1, -2n, Greater],
    [0, 2n, Less],
    [0, 1n, Less],
    [0, 0n, Equal],
    [0, -1n, Greater],
    [0, -2n, Greater],
    [-1, 2n, Less],
    [-1, 1n, Less],
    [-1, 0n, Less],
    [-1, -1n, Equal],
    [-1, -2n, Greater],

    [1n, 2, Less],
    [1n, 1, Equal],
    [1n, 0, Greater],
    [1n, -1, Greater],
    [1n, -2, Greater],
    [0n, 2, Less],
    [0n, 1, Less],
    [0n, 0, Equal],
    [0n, -1, Greater],
    [0n, -2, Greater],
    [-1n, 2, Less],
    [-1n, 1, Less],
    [-1n, 0, Less],
    [-1n, -1, Equal],
    [-1n, -2, Greater],

    [createHeapBigInt(1000000000000000000000000n), 2, Greater],
    [createHeapBigInt(1000000000000000000000000n), 1, Greater],
    [createHeapBigInt(1000000000000000000000000n), 0, Greater],
    [createHeapBigInt(1000000000000000000000000n), -1, Greater],
    [createHeapBigInt(1000000000000000000000000n), -2, Greater],
    [createHeapBigInt(-1000000000000000000000000n), 2, Less],
    [createHeapBigInt(-1000000000000000000000000n), 1, Less],
    [createHeapBigInt(-1000000000000000000000000n), 0, Less],
    [createHeapBigInt(-1000000000000000000000000n), -1, Less],
    [createHeapBigInt(-1000000000000000000000000n), -2, Less],

    [2, createHeapBigInt(1000000000000000000000000n), Less],
    [1, createHeapBigInt(1000000000000000000000000n), Less],
    [0, createHeapBigInt(1000000000000000000000000n), Less],
    [-1, createHeapBigInt(1000000000000000000000000n), Less],
    [-2, createHeapBigInt(1000000000000000000000000n), Less],
    [2, createHeapBigInt(-1000000000000000000000000n), Greater],
    [1, createHeapBigInt(-1000000000000000000000000n), Greater],
    [0, createHeapBigInt(-1000000000000000000000000n), Greater],
    [-1, createHeapBigInt(-1000000000000000000000000n), Greater],
    [-2, createHeapBigInt(-1000000000000000000000000n), Greater],

    [1.5, 2n, Less],
    [1.0, 1n, Equal],
    [1.5, 0n, Greater],
    [1.5, -1n, Greater],
    [1.5, -2n, Greater],
    [0.5, 2n, Less],
    [0.5, 1n, Less],
    [-0.0, 0n, Equal],
    [0.0, 0n, Equal],
    [0.5, -1n, Greater],
    [0.5, -2n, Greater],
    [-1.5, 2n, Less],
    [-1.5, 1n, Less],
    [-1.5, 0n, Less],
    [-1.0, -1n, Equal],
    [-1.5, -2n, Greater],

    [1n, 2.5, Less],
    [1n, 1.0, Equal],
    [1n, 0.5, Greater],
    [1n, -1.5, Greater],
    [1n, -2.5, Greater],
    [0n, 2.5, Less],
    [0n, 1.0, Less],
    [0n, 0.0, Equal],
    [0n, -0.0, Equal],
    [0n, -1.5, Greater],
    [0n, -2.5, Greater],
    [-1n, 2.5, Less],
    [-1n, 1.0, Less],
    [-1n, 0.5, Less],
    [-1n, -1.0, Equal],
    [-1n, -2.5, Greater],

    [createHeapBigInt(1n), 2.5, Less],
    [createHeapBigInt(1n), 1.0, Equal],
    [createHeapBigInt(1n), 0.5, Greater],
    [createHeapBigInt(1n), -1.5, Greater],
    [createHeapBigInt(1n), -2.5, Greater],
    [createHeapBigInt(0n), 2.5, Less],
    [createHeapBigInt(0n), 1.5, Less],
    [createHeapBigInt(0n), 0.0, Equal],
    [createHeapBigInt(0n), -0.0, Equal],
    [createHeapBigInt(0n), -1.5, Greater],
    [createHeapBigInt(0n), -2.5, Greater],
    [createHeapBigInt(-1n), 2.5, Less],
    [createHeapBigInt(-1n), 1.5, Less],
    [createHeapBigInt(-1n), 0.5, Less],
    [createHeapBigInt(-1n), -1.0, Equal],
    [createHeapBigInt(-1n), -2.5, Greater],

    [1.5, createHeapBigInt(2n), Less],
    [1.0, createHeapBigInt(1n), Equal],
    [1.5, createHeapBigInt(0n), Greater],
    [1.5, createHeapBigInt(-1n), Greater],
    [1.5, createHeapBigInt(-2n), Greater],
    [0.5, createHeapBigInt(2n), Less],
    [0.5, createHeapBigInt(1n), Less],
    [0.0, createHeapBigInt(0n), Equal],
    [-0.0, createHeapBigInt(0n), Equal],
    [0.5, createHeapBigInt(-1n), Greater],
    [0.5, createHeapBigInt(-2n), Greater],
    [-1.5, createHeapBigInt(2n), Less],
    [-1.5, createHeapBigInt(1n), Less],
    [-1.5, createHeapBigInt(0n), Less],
    [-1.0, createHeapBigInt(-1n), Equal],
    [-1.5, createHeapBigInt(-2n), Greater],

    [1n, NaN, Undefined],
    [0n, NaN, Undefined],
    [-1n, NaN, Undefined],

    [NaN, 1n, Undefined],
    [NaN, 0n, Undefined],
    [NaN, -1n, Undefined],

    [createHeapBigInt(1n), NaN, Undefined],
    [createHeapBigInt(0n), NaN, Undefined],
    [createHeapBigInt(-1n), NaN, Undefined],

    [NaN, createHeapBigInt(1n), Undefined],
    [NaN, createHeapBigInt(0n), Undefined],
    [NaN, createHeapBigInt(-1n), Undefined],

    [1n, Infinity, Less],
    [0n, Infinity, Less],
    [-1n, Infinity, Less],

    [Infinity, 1n, Greater],
    [Infinity, 0n, Greater],
    [Infinity, -1n, Greater],

    [createHeapBigInt(1n), Infinity, Less],
    [createHeapBigInt(0n), Infinity, Less],
    [createHeapBigInt(-1n), Infinity, Less],

    [Infinity, createHeapBigInt(1n), Greater],
    [Infinity, createHeapBigInt(0n), Greater],
    [Infinity, createHeapBigInt(-1n), Greater],

    [1n, -Infinity, Greater],
    [0n, -Infinity, Greater],
    [-1n, -Infinity, Greater],

    [-Infinity, 1n, Less],
    [-Infinity, 0n, Less],
    [-Infinity, -1n, Less],

    [createHeapBigInt(1n), -Infinity, Greater],
    [createHeapBigInt(0n), -Infinity, Greater],
    [createHeapBigInt(-1n), -Infinity, Greater],

    [-Infinity, createHeapBigInt(1n), Less],
    [-Infinity, createHeapBigInt(0n), Less],
    [-Infinity, createHeapBigInt(-1n), Less],

    [createHeapBigInt(1000000000000000000000000n), Infinity, Less],
    [Infinity, createHeapBigInt(1000000000000000000000000n), Greater],
    [createHeapBigInt(1000000000000000000000000n), -Infinity, Greater],
    [-Infinity, createHeapBigInt(1000000000000000000000000n), Less],

    [createHeapBigInt(-1000000000000000000000000n), Infinity, Less],
    [Infinity, createHeapBigInt(-1000000000000000000000000n), Greater],
    [createHeapBigInt(-1000000000000000000000000n), -Infinity, Greater],
    [-Infinity, createHeapBigInt(-1000000000000000000000000n), Less],
];

function lessThan(a, b)
{
    return a < b;
}
noInline(lessThan);

function lessThanEqual(a, b)
{
    return a <= b;
}
noInline(lessThanEqual);

function greaterThan(a, b)
{
    return a > b;
}
noInline(greaterThan);

function greaterThanEqual(a, b)
{
    return a >= b;
}
noInline(greaterThanEqual);

function equal(a, b)
{
    return a == b;
}
noInline(equal);

for (var i = 0; i < 1e3; ++i) {
    for (let [a, b, res] of list) {
        shouldBe(lessThan(a, b), result(lessThan, res));
        shouldBe(lessThanEqual(a, b), result(lessThanEqual, res));
        shouldBe(greaterThan(a, b), result(greaterThan, res));
        shouldBe(greaterThanEqual(a, b), result(greaterThanEqual, res));
        shouldBe(equal(a, b), result(equal, res));
    }
}
