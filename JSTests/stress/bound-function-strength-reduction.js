function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(a, b, c) {
    return a + b + c;
}
noInline(test);

var bound = test.bind(0, 1);
noInline(bound);

function t1() {
    return bound(2, 3, 4);
}
noInline(t1);

function t2() {
    return bound('hello');
}
noInline(t2);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(t1(), 6);
    shouldBe(t2(), "1helloundefined");
}
