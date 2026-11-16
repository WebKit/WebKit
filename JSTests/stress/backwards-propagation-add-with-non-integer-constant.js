function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

function addThenAddHalf(x, y) {
    return (x + y + 0.5) | 0;
}
noInline(addThenAddHalf);

function addThenAddNegativeHalf(x, y) {
    return (x + y + -0.5) | 0;
}
noInline(addThenAddNegativeHalf);

function subThenAddHalf(x, y) {
    return (x - y + 0.5) | 0;
}
noInline(subThenAddHalf);

function halfOnLeft(x, y) {
    return (0.5 + (x + y)) | 0;
}
noInline(halfOnLeft);

function unsignedRightShift(x, y) {
    return (x + y + 0.5) >>> 0;
}
noInline(unsignedRightShift);

function bitAnd(x, y) {
    return (x + y + 0.5) & 0xff;
}
noInline(bitAnd);

function threeOperands(x, y, z) {
    return (x + y + z + 0.25) | 0;
}
noInline(threeOperands);

function negate(x, y) {
    return (-((x >>> 0) + (y >>> 0)) + 0.5) | 0;
}
noInline(negate);

function mulThenAddHalf(x) {
    return (x * 100 + 0.5) | 0;
}
noInline(mulThenAddHalf);

function largeNegativeConstant(x) {
    return ((x | 0) + -4294967296 + 1.5) | 0;
}
noInline(largeNegativeConstant);

function largePositiveConstant(x) {
    return ((x | 0) + 4294967297 + 0.5) | 0;
}
noInline(largePositiveConstant);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(addThenAddHalf(1, 2), 3);
    shouldBe(addThenAddHalf(2000000000, 2000000000), -294967296);
    shouldBe(addThenAddHalf(-2000000000, -2000000000), 294967297);
    shouldBe(addThenAddNegativeHalf(1, 2), 2);
    shouldBe(addThenAddNegativeHalf(2000000000, 2000000000), -294967297);
    shouldBe(addThenAddNegativeHalf(-2000000000, -2000000000), 294967296);
    shouldBe(subThenAddHalf(1, 2), 0);
    shouldBe(subThenAddHalf(2000000000, -2000000000), -294967296);
    shouldBe(halfOnLeft(1, 2), 3);
    shouldBe(halfOnLeft(2000000000, 2000000000), -294967296);
    shouldBe(unsignedRightShift(1, 2), 3);
    shouldBe(unsignedRightShift(2000000000, 2000000000), 4000000000);
    shouldBe(bitAnd(1, 2), 3);
    shouldBe(bitAnd(2000000000, 2000000000), 0);
    shouldBe(threeOperands(1, 2, 3), 6);
    shouldBe(threeOperands(1000000000, 1000000000, 2000000000), -294967296);
    shouldBe(negate(1, 2), -2);
    shouldBe(negate(2000000000, 2000000000), 294967297);
    shouldBe(mulThenAddHalf(1), 100);
    shouldBe(mulThenAddHalf(30000000), -1294967296);
    shouldBe(largeNegativeConstant(0), 2);
    shouldBe(largePositiveConstant(-2), -1);
}
