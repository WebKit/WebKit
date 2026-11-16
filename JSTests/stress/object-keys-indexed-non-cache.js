function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(object)
{
    return Object.keys(object);
}
noInline(test);

var object = {0: 42};
for (var i = 0; i < 1e3; ++i) {
    var result = test(object);
    shouldBe(result.length, 1);
    shouldBe(result[0], '0');
}
object[1] = 44;
for (var i = 0; i < 1e3; ++i) {
    var result = test(object);
    shouldBe(result.length, 2);
    shouldBe(result[0], '0');
    shouldBe(result[1], '1');
}
