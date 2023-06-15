function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function parseIntDouble(value) {
    return parseInt(value);
}
noInline(parseIntDouble);

for (var i = 0; i < 1e5; ++i) {
    shouldBe(Object.is(parseIntDouble(-0.0), 0), true); // Not -0 since -0.0.toString() is "0".
    shouldBe(Object.is(parseIntDouble(-1.0), -1.0), true);
    shouldBe(Object.is(parseIntDouble(-0.01), -0), true);
    shouldBe(Object.is(parseIntDouble(-1.1), -1.0), true);
    shouldBe(Object.is(parseIntDouble(-1.0), -1.0), true);
    shouldBe(Object.is(parseIntDouble(-0.9), -0.0), true);
    shouldBe(Object.is(parseIntDouble(-1.000000001), -1.0), true);
    shouldBe(Object.is(parseIntDouble(-0.000000001), -1), true); // Since it is -1e-9.
    shouldBe(Object.is(parseIntDouble(0.000000001), 1), true); // Since it is 1e-9.
    shouldBe(Object.is(parseIntDouble(0.000001), 0), true);
    shouldBe(Object.is(parseIntDouble(0.000001), 0), true);
    shouldBe(Object.is(parseIntDouble(0.0000001), 1), true); // Since it is 1e-6.
    shouldBe(Object.is(parseIntDouble(-0.0000001), -1), true); // Since it is -1e-6.
    shouldBe(Object.is(parseIntDouble(1e+21), 1), true); // Since it is 1e+21.
    shouldBe(Object.is(parseIntDouble(1e+20), 1e+20), true);
    shouldBe(Object.is(parseIntDouble(NaN), NaN), true);
    shouldBe(Object.is(parseIntDouble(Infinity), NaN), true);
    shouldBe(Object.is(parseIntDouble(-Infinity), NaN), true);
}
