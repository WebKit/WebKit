function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function parseFloatDouble(value) {
    return parseFloat(value);
}
noInline(parseFloatDouble);

for (var i = 0; i < 1e5; ++i) {
    shouldBe(Object.is(parseFloatDouble(0.0), 0), true);
    shouldBe(Object.is(parseFloatDouble(-0.0), 0), true); // Not -0 since -0.0.toString() is "0".
    shouldBe(Object.is(parseFloatDouble(-1.0), -1.0), true);
    shouldBe(Object.is(parseFloatDouble(-0.01), -0.01), true);
    shouldBe(Object.is(parseFloatDouble(-1.1), -1.1), true);
    shouldBe(Object.is(parseFloatDouble(-1.0), -1.0), true);
    shouldBe(Object.is(parseFloatDouble(-0.9), -0.9), true);
    shouldBe(Object.is(parseFloatDouble(-1.000000001), -1.000000001), true);
    shouldBe(Object.is(parseFloatDouble(-0.000000001), -0.000000001), true);
    shouldBe(Object.is(parseFloatDouble(0.000000001), 0.000000001), true);
    shouldBe(Object.is(parseFloatDouble(0.000001), 0.000001), true);
    shouldBe(Object.is(parseFloatDouble(0.000001), 0.000001), true);
    shouldBe(Object.is(parseFloatDouble(0.0000001), 0.0000001), true);
    shouldBe(Object.is(parseFloatDouble(-0.0000001), -0.0000001), true);
    shouldBe(Object.is(parseFloatDouble(1e+21), 1e+21), true);
    shouldBe(Object.is(parseFloatDouble(1e+20), 1e+20), true);
    shouldBe(Object.is(parseFloatDouble(NaN), NaN), true);
    shouldBe(Object.is(parseFloatDouble(Infinity), Infinity), true);
    shouldBe(Object.is(parseFloatDouble(-Infinity), -Infinity), true);
}
