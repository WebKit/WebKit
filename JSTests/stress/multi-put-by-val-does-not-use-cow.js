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

function test(array) {
    array[0] = 42;
    return array;
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBeArray(test([0.1, 0.2, 0.3, 0.4]), [42, 0.2, 0.3, 0.4]);
    shouldBeArray(test(new Uint32Array([0, 1, 2, 3])), [42, 1, 2, 3]);
}
