function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function caller(callback, value) {
    return callback(value) + 1;
}
noInline(caller);

function warmUp(round) {
    let scale = round + 2;
    let callback = (value) => value * scale;
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(caller(callback, i), i * scale + 1);
}
noInline(warmUp);

for (let round = 0; round < 8; ++round) {
    warmUp(round);
    fullGC();
    shouldBe(caller((value) => value - round, 10), 11 - round);
}
