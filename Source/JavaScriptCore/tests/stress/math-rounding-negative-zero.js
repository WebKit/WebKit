function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testRound(value)
{
    return Math.round(value);
}
noInline(testRound);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(1 / testRound(-0.4), -Infinity);
    shouldBe(1 / testRound(-0.5), -Infinity);
    shouldBe(1 / testRound(-0.6), -1.0);
    shouldBe(1 / testRound(-0.0), -Infinity);
    shouldBe(1 / testRound(0.1), Infinity);
}

function testFloor(value)
{
    return Math.floor(value);
}
noInline(testFloor);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(1 / testFloor(-0.0), -Infinity);
}

function testCeil(value)
{
    return Math.ceil(value);
}
noInline(testCeil);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(1 / testCeil(-0.0), -Infinity);
    shouldBe(1 / testCeil(-0.9), -Infinity);
}

function testRoundNonNegativeZero(value)
{
    return Math.round(value) | 0;
}
noInline(testRoundNonNegativeZero);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(testRoundNonNegativeZero(0.4), 0);
    shouldBe(testRoundNonNegativeZero(0.5), 1);
    shouldBe(testRoundNonNegativeZero(0.6), 1);
    shouldBe(testRoundNonNegativeZero(0.0), 0);
    shouldBe(testRoundNonNegativeZero(0.1), 0);
}
shouldBe(1 / testRoundNonNegativeZero(-0.4), Infinity);

function testRoundNonNegativeZero2(value)
{
    return Math.round(value) | 0;
}
noInline(testRoundNonNegativeZero2);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(1 / testRoundNonNegativeZero2(-0.4), Infinity);
    shouldBe(1 / testRoundNonNegativeZero2(-0.5), Infinity);
    shouldBe(1 / testRoundNonNegativeZero2(-0.6), -1.0);
    shouldBe(1 / testRoundNonNegativeZero2(-0.0), Infinity);
    shouldBe(1 / testRoundNonNegativeZero2(0.1), Infinity);
}


