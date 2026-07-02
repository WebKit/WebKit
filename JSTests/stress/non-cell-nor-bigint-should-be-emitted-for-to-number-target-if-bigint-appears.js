function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(value) {
    return Math.abs(value);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(test(null), 0);
    shouldBe(test(false), 0);
    shouldBe(test(0.5), 0.5);
}

for (var i = 0; i < 1e2; ++i) {
    shouldThrow(() => {
        test(2n);
    }, `TypeError: Conversion from 'BigInt' to 'number' is not allowed.`);
    shouldThrow(() => {
        test(0xf00000n);
    }, `TypeError: Conversion from 'BigInt' to 'number' is not allowed.`);
    shouldThrow(() => {
        test(0xffffffffffffffffffffffffffn);
    }, `TypeError: Conversion from 'BigInt' to 'number' is not allowed.`);
}
