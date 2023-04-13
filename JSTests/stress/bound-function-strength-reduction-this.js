function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {};

function test(a, b, c) {
    return this;
}
noInline(test);

var bound = test.bind(object, 1);
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
    shouldBe(bound(1, 2), object);
    shouldBe(t1(), object);
    shouldBe(t2(), object);
}
