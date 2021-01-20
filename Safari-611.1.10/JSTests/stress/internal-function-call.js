function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var func = Date;

for (var i = 0; i < 1e4; ++i)
    shouldBe(typeof func(), "string");
