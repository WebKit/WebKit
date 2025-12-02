function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(size) {
    let result = 0;
    let ab = new ArrayBuffer(size);
    let view = new Uint8Array(ab);
    for (let i = 0; i < size; ++i) {
        view[i] = i;
        result += i;
    }
    return result;
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test(128), 8128);
