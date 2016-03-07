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
    shouldBe(testRound(Infinity), Infinity);
    shouldBe(testRound(-Infinity), -Infinity);
    shouldBe(testFloor(Infinity), Infinity);
    shouldBe(testFloor(-Infinity), -Infinity);
    shouldBe(testCeil(Infinity), Infinity);
    shouldBe(testCeil(-Infinity), -Infinity);
}
