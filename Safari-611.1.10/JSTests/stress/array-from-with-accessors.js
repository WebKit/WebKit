function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [0, 1, 2, 3, 4, 5];
Object.defineProperty(Array.prototype, '0', {
    get() {
        throw new Error('cannot get to 0 getter');
    },
    set() {
        throw new Error('cannot put to 0 setter');
    }
});

var result = Array.from(array);
shouldBe(result.length, array.length);
shouldBe(result instanceof Array, true);

for (var i = 0; i < array.length; ++i)
    shouldBe(result[i], array[i]);

