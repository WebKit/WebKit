function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testing(object) {
    var value = object[1000];
    shouldBe(object[1000], 42);
}

testing({
    0: 0,
    1: 1,
    1000: 42
});

var object = {
    0: 0,
    get 1000() {
        return 42;
    },
    1: 1,
};
testing(object);
shouldBe(object[0], 0);
shouldBe(object[1], 1);
