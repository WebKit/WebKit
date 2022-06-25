function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function forIn(object)
{
    var iteration = 0;
    for (var i in object)
        ++iteration;
    return iteration;
}
noInline(forIn);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(forIn(null), 0);
    shouldBe(forIn(undefined), 0);
}
