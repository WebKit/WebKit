function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

for (var i = 0; i < 10; ++i) {
    Object.defineProperty(Array.prototype, i, {
        get() {
            throw new Error('get is called.');
        },
        set(value) {
            throw new Error('set is called.');
        }
    });
}

var original = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];

// Doesn't throw any errors.
var mapped = original.map(function (value) {
    return value * 2;
});

shouldBe(mapped.length, 10);
for (var i = 0; i < 10; ++i) {
    shouldBe(mapped[i], i * 2);
}
