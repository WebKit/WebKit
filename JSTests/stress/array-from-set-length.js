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

class ArrayLike {
    constructor(length) {
        this.lengthCalled = false;
        this._length = length;
    }
    set length(value) {
        this.lengthCalled = true;
        this._length = value;
    }
    get length() {
        return this._length;
    }
}

var arrayLike = new ArrayLike(10);
for (var i = 0; i < 10; ++i) {
    arrayLike[i] = i;
}
shouldBe(arrayLike.lengthCalled, false);

var generated = Array.from.call(ArrayLike, arrayLike);

shouldBe(generated.length, 10);
shouldBe(generated instanceof ArrayLike, true);
for (var i = 0; i < 10; ++i) {
    shouldBe(generated[i], i);
}
shouldBe(arrayLike.lengthCalled, false);
shouldBe(generated.lengthCalled, true);
