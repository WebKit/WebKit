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
    return value >>> 3;
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(test(null), 0);
    shouldBe(test(false), 0);
    shouldBe(test(10000000), 1250000);
}

for (var i = 0; i < 1e2; ++i) {
    shouldThrow(() => {
        test(2n);
    }, `TypeError: BigInt does not support >>> operator`);
    shouldThrow(() => {
        test(0xf00000n);
    }, `TypeError: BigInt does not support >>> operator`);
    shouldThrow(() => {
        test(0xffffffffffffffffffffffffffn);
    }, `TypeError: BigInt does not support >>> operator`);
}
