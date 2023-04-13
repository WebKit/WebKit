function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(a, b, c)
{
    return a + b + c;
}
var bound1 = test.bind(undefined, 0, 1, 2);
var bound2 = bound1.bind(undefined);

for (var i = 0; i < 1e6; ++i)
    shouldBe(bound2(), 3);
