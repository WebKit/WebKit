function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testRound(value)
{
    return Math.round(value);
}
noInline(testRound);

function testFloor(value)
{
    return Math.floor(value);
}
noInline(testFloor);

function testCeil(value)
{
    return Math.ceil(value);
}
noInline(testCeil);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(Number.isNaN(testRound(NaN)), true);
    shouldBe(Number.isNaN(testFloor(NaN)), true);
    shouldBe(Number.isNaN(testCeil(NaN)), true);
}
