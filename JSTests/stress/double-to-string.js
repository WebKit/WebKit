function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(num) {
    return num + 'px';
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test(i + 30.4), (i + 30.4) + 'px');
