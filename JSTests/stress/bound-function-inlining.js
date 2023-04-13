function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(a, b, c) {
    return a + b + c;
}

var bound = test.bind(0, 1);

function t() {
    return bound(2, 3, 4);
}
noInline(t);
for (var i = 0; i < 1e6; ++i)
    shouldBe(t(), 6);
