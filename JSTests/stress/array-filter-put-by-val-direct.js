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
var filtered = original.filter(function (value, index) {
    return index % 2 == 0;
});

shouldBe(filtered.length, 5);
for (var i = 0; i < 5; ++i) {
    shouldBe(filtered[i], i * 2);
}
