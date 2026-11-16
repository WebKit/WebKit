function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const Greater = 0;
const Less = 1;
const Equal = 2;

function result(func, data)
{
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
    [createHeapBigInt(1n), createHeapBigInt(2n), Less],
    [createHeapBigInt(1n), createHeapBigInt(1n), Equal],
    [createHeapBigInt(1n), createHeapBigInt(0n), Greater],
    [createHeapBigInt(1n), createHeapBigInt(-1n), Greater],
    [createHeapBigInt(1n), createHeapBigInt(-2n), Greater],
    [createHeapBigInt(0n), createHeapBigInt(2n), Less],
    [createHeapBigInt(0n), createHeapBigInt(1n), Less],
    [createHeapBigInt(0n), createHeapBigInt(0n), Equal],
    [createHeapBigInt(0n), createHeapBigInt(-1n), Greater],
    [createHeapBigInt(0n), createHeapBigInt(-2n), Greater],
    [createHeapBigInt(-1n), createHeapBigInt(2n), Less],
    [createHeapBigInt(-1n), createHeapBigInt(1n), Less],
    [createHeapBigInt(-1n), createHeapBigInt(0n), Less],
    [createHeapBigInt(-1n), createHeapBigInt(-1n), Equal],
    [createHeapBigInt(-1n), createHeapBigInt(-2n), Greater],

    [createHeapBigInt(1000000000000000000000000n), createHeapBigInt(2n), Greater],
    [createHeapBigInt(1000000000000000000000000n), createHeapBigInt(1n), Greater],
    [createHeapBigInt(1000000000000000000000000n), createHeapBigInt(0n), Greater],
    [createHeapBigInt(1000000000000000000000000n), createHeapBigInt(-1n), Greater],
    [createHeapBigInt(1000000000000000000000000n), createHeapBigInt(-2n), Greater],
    [createHeapBigInt(-1000000000000000000000000n), createHeapBigInt(2n), Less],
    [createHeapBigInt(-1000000000000000000000000n), createHeapBigInt(1n), Less],
    [createHeapBigInt(-1000000000000000000000000n), createHeapBigInt(0n), Less],
    [createHeapBigInt(-1000000000000000000000000n), createHeapBigInt(-1n), Less],
    [createHeapBigInt(-1000000000000000000000000n), createHeapBigInt(-2n), Less],

    [createHeapBigInt(1000000000000000000000000n), createHeapBigInt(1000000000000000000000002n), Less],
    [createHeapBigInt(1000000000000000000000001n), createHeapBigInt(1000000000000000000000000n), Greater],
    [createHeapBigInt(1000000000000000000000000n), createHeapBigInt(1000000000000000000000000n), Equal],
    [createHeapBigInt(-1000000000000000000000000n), createHeapBigInt(-1000000000000000000000000n), Equal],
    [createHeapBigInt(1000000000000000000000000n), createHeapBigInt(-1000000000000000000000001n), Greater],
    [createHeapBigInt(-1000000000000000000000000n), createHeapBigInt(1000000000000000000000002n), Less],
    [createHeapBigInt(-1000000000000000000000000n), createHeapBigInt(1000000000000000000000001n), Less],
    [createHeapBigInt(-1000000000000000000000000n), createHeapBigInt(1000000000000000000000000n), Less],
    [createHeapBigInt(-1000000000000000000000000n), createHeapBigInt(-1000000000000000000000001n), Greater],
    [createHeapBigInt(-1000000000000000000000000n), createHeapBigInt(-1000000000000000000000002n), Greater],
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
