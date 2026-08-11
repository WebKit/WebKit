function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.toString(), expected.toString());
}

function shouldThrow(callback, errorConstructor) {
    try {
        callback();
    } catch (e) {
        shouldBe(e instanceof errorConstructor, true);
        return;
    }
    throw new Error('FAIL: should have thrown');
}

var target = new Uint8Array([255, 255, 255, 255, 255]);
shouldThrow(() => {
    target.setFromBase64("ABCD=EFG");
}, SyntaxError);
shouldBeArray(target, [0, 16, 131, 255, 255]);

target = new Uint8Array([255, 255, 255, 255, 255]);
shouldThrow(() => {
    target.setFromBase64("ABCD$EFGH");
}, SyntaxError);
shouldBeArray(target, [0, 16, 131, 255, 255]);

target = new Uint8Array([255, 255, 255, 255, 255]);
shouldThrow(() => {
    target.setFromBase64("ABCD==FG");
}, SyntaxError);
shouldBeArray(target, [0, 16, 131, 255, 255]);
