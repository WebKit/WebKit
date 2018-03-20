function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array)
{
    return array.slice(0);
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    var array = [i, i, i];
    var result = test(array);
    shouldBe(array !== result, true);
    shouldBe(result.length, 3);
    for (var j = 0; j < 3; ++j)
        shouldBe(result[j], i);
}

function test2(array, i)
{
    return array.slice(0, i);
}
noInline(test2);

for (var i = 0; i < 1e5; ++i) {
    var array = [i, i, i];
    var result = test2(array, 2);
    shouldBe(array !== result, true);
    shouldBe(result.length, 2);
    for (var j = 0; j < 2; ++j)
        shouldBe(result[j], i);
}
