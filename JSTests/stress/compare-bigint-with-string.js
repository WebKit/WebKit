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
    [createHeapBigInt(1n), "2", Less],
    [createHeapBigInt(1n), "1", Equal],
    [createHeapBigInt(1n), "0", Greater],
    [createHeapBigInt(1n), "-1", Greater],
    [createHeapBigInt(1n), "-2", Greater],
    [createHeapBigInt(0n), "2", Less],
    [createHeapBigInt(0n), "1", Less],
    [createHeapBigInt(0n), "0", Equal],
    [createHeapBigInt(0n), "-1", Greater],
    [createHeapBigInt(0n), "-2", Greater],
    [createHeapBigInt(-1n), "2", Less],
    [createHeapBigInt(-1n), "1", Less],
    [createHeapBigInt(-1n), "0", Less],
    [createHeapBigInt(-1n), "-1", Equal],
    [createHeapBigInt(-1n), "-2", Greater],

    ["1", createHeapBigInt(2n), Less],
    ["1", createHeapBigInt(1n), Equal],
    ["1", createHeapBigInt(0n), Greater],
    ["1", createHeapBigInt(-1n), Greater],
    ["1", createHeapBigInt(-2n), Greater],
    ["0", createHeapBigInt(2n), Less],
    ["0", createHeapBigInt(1n), Less],
    ["0", createHeapBigInt(0n), Equal],
    ["0", createHeapBigInt(-1n), Greater],
    ["0", createHeapBigInt(-2n), Greater],
    ["-1", createHeapBigInt(2n), Less],
    ["-1", createHeapBigInt(1n), Less],
    ["-1", createHeapBigInt(0n), Less],
    ["-1", createHeapBigInt(-1n), Equal],
    ["-1", createHeapBigInt(-2n), Greater],

    ["1", 2n, Less],
    ["1", 1n, Equal],
    ["1", 0n, Greater],
    ["1", -1n, Greater],
    ["1", -2n, Greater],
    ["0", 2n, Less],
    ["0", 1n, Less],
    ["0", 0n, Equal],
    ["0", -1n, Greater],
    ["0", -2n, Greater],
    ["-1", 2n, Less],
    ["-1", 1n, Less],
    ["-1", 0n, Less],
    ["-1", -1n, Equal],
    ["-1", -2n, Greater],

    [1n, "2", Less],
    [1n, "1", Equal],
    [1n, "0", Greater],
    [1n, "-1", Greater],
    [1n, "-2", Greater],
    [0n, "2", Less],
    [0n, "1", Less],
    [0n, "0", Equal],
    [0n, "-1", Greater],
    [0n, "-2", Greater],
    [-1n, "2", Less],
    [-1n, "1", Less],
    [-1n, "0", Less],
    [-1n, "-1", Equal],
    [-1n, "-2", Greater],

    [createHeapBigInt(1000000000000000000000000n), "2", Greater],
    [createHeapBigInt(1000000000000000000000000n), "1", Greater],
    [createHeapBigInt(1000000000000000000000000n), "0", Greater],
    [createHeapBigInt(1000000000000000000000000n), "-1", Greater],
    [createHeapBigInt(1000000000000000000000000n), "-2", Greater],
    [createHeapBigInt(-1000000000000000000000000n), "2", Less],
    [createHeapBigInt(-1000000000000000000000000n), "1", Less],
    [createHeapBigInt(-1000000000000000000000000n), "0", Less],
    [createHeapBigInt(-1000000000000000000000000n), "-1", Less],
    [createHeapBigInt(-1000000000000000000000000n), "-2", Less],

    ["2", createHeapBigInt(1000000000000000000000000n), Less],
    ["1", createHeapBigInt(1000000000000000000000000n), Less],
    ["0", createHeapBigInt(1000000000000000000000000n), Less],
    ["-1", createHeapBigInt(1000000000000000000000000n), Less],
    ["-2", createHeapBigInt(1000000000000000000000000n), Less],
    ["2", createHeapBigInt(-1000000000000000000000000n), Greater],
    ["1", createHeapBigInt(-1000000000000000000000000n), Greater],
    ["0", createHeapBigInt(-1000000000000000000000000n), Greater],
    ["-1", createHeapBigInt(-1000000000000000000000000n), Greater],
    ["-2", createHeapBigInt(-1000000000000000000000000n), Greater],

    ["1.5", 2n, Undefined],
    ["1.0", 1n, Undefined],
    ["1.5", 0n, Undefined],
    ["1.5", -1n, Undefined],
    ["1.5", -2n, Undefined],
    ["0.5", 2n, Undefined],
    ["0.5", 1n, Undefined],
    ["-0.0", 0n, Undefined],
    ["0.0", 0n, Undefined],
    ["0.5", -1n, Undefined],
    ["0.5", -2n, Undefined],
    ["-1.5", 2n, Undefined],
    ["-1.5", 1n, Undefined],
    ["-1.5", 0n, Undefined],
    ["-1.0", -1n, Undefined],
    ["-1.5", -2n, Undefined],

    [1n, "2.5", Undefined],
    [1n, "1.0", Undefined],
    [1n, "0.5", Undefined],
    [1n, "-1.5", Undefined],
    [1n, "-2.5", Undefined],
    [0n, "2.5", Undefined],
    [0n, "1.0", Undefined],
    [0n, "0.0", Undefined],
    [0n, "-0.0", Undefined],
    [0n, "-1.5", Undefined],
    [0n, "-2.5", Undefined],
    [-1n, "2.5", Undefined],
    [-1n, "1.0", Undefined],
    [-1n, "0.5", Undefined],
    [-1n, "-1.0", Undefined],
    [-1n, "-2.5", Undefined],

    [createHeapBigInt(1n), "2.5", Undefined],
    [createHeapBigInt(1n), "1.0", Undefined],
    [createHeapBigInt(1n), "0.5", Undefined],
    [createHeapBigInt(1n), "-1.5", Undefined],
    [createHeapBigInt(1n), "-2.5", Undefined],
    [createHeapBigInt(0n), "2.5", Undefined],
    [createHeapBigInt(0n), "1.5", Undefined],
    [createHeapBigInt(0n), "0.0", Undefined],
    [createHeapBigInt(0n), "-0.0", Undefined],
    [createHeapBigInt(0n), "-1.5", Undefined],
    [createHeapBigInt(0n), "-2.5", Undefined],
    [createHeapBigInt(-1n), "2.5", Undefined],
    [createHeapBigInt(-1n), "1.5", Undefined],
    [createHeapBigInt(-1n), "0.5", Undefined],
    [createHeapBigInt(-1n), "-1.0", Undefined],
    [createHeapBigInt(-1n), "-2.5", Undefined],

    ["1.5", createHeapBigInt(2n), Undefined],
    ["1.0", createHeapBigInt(1n), Undefined],
    ["1.5", createHeapBigInt(0n), Undefined],
    ["1.5", createHeapBigInt(-1n), Undefined],
    ["1.5", createHeapBigInt(-2n), Undefined],
    ["0.5", createHeapBigInt(2n), Undefined],
    ["0.5", createHeapBigInt(1n), Undefined],
    ["0.0", createHeapBigInt(0n), Undefined],
    ["-0.0", createHeapBigInt(0n), Undefined],
    ["0.5", createHeapBigInt(-1n), Undefined],
    ["0.5", createHeapBigInt(-2n), Undefined],
    ["-1.5", createHeapBigInt(2n), Undefined],
    ["-1.5", createHeapBigInt(1n), Undefined],
    ["-1.5", createHeapBigInt(0n), Undefined],
    ["-1.0", createHeapBigInt(-1n), Undefined],
    ["-1.5", createHeapBigInt(-2n), Undefined],

    [createHeapBigInt(1000000000000000000000000n), "100000000000000000000000000000000000", Less],
    ["100000000000000000000000000000000000", createHeapBigInt(1000000000000000000000000n), Greater],
    [createHeapBigInt(1000000000000000000000000n), "-100000000000000000000000000000000000", Greater],
    ["-100000000000000000000000000000000000", createHeapBigInt(1000000000000000000000000n), Less],
    [createHeapBigInt(1000000000000000000000000n), "1000000000000000000000000", Equal],
    ["1000000000000000000000000", createHeapBigInt(1000000000000000000000000n), Equal],
    [createHeapBigInt(-1000000000000000000000000n), "-1000000000000000000000000", Equal],
    ["-1000000000000000000000000", createHeapBigInt(-1000000000000000000000000n), Equal],

    [20n, "100000000000000000000000000000000000", Less],
    ["100000000000000000000000000000000000", 20n, Greater],
    [20n, "-100000000000000000000000000000000000", Greater],
    ["-100000000000000000000000000000000000", 20n, Less],
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
