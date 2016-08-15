function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var obj = {
    0: 0,
    1: 1,
    get 1() {
        return 42;
    }
};

function testing(object) {
    shouldBe(object[1], 42);
    shouldBe(obj[0], 0);
}
noInline(testing);

for (var i = 0; i < 10000; ++i)
    testing(obj);
