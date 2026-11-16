function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array) {
    array.splice(0, 3);
}
noInline(test);

var count = 0;

for (var i = 0; i < 1e4; ++i) {
    var array = [0, 1, 2, 3];
    var set = null;
    Reflect.defineProperty(array, 0, {
        get() {
            ++count;
            return 42;
        },
        set(value) {
            set = value;
        },
        configurable: true,
    });
    test(array);
    shouldBe(array.length, 1);
    shouldBe(set, 3);
}
shouldBe(count, 1e4);
