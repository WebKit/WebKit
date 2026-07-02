function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(value) {
    return Promise.resolve(1).then(function (v) { return value * value * v; });
}
noInline(test);

(async function () {
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(await test(i), i * i);
}());
