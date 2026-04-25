function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        try {
            shouldBe(actual[i], expected[i]);
        } catch(e) {
            print(JSON.stringify(actual));
            throw e;
        }
    }
}

let target = new Uint8Array([255, 255, 255, 255, 255]);
try {
    target.setFromBase64('MjYyZg===');
} catch { }
shouldBeArray(target, [50, 54, 50, 255, 255]);
