function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array)
{
    return array.slice();
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    var array = [i, i, i];
    var result = test(array);
    shouldBe(array !== result, true);
    shouldBe(result.length, 3);
    for (var j = 0; j < 3; ++j)
        shouldBe(result[j], i);
}
