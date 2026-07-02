function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = new Uint8Array(1024);
Uint8Array.__proto__ = {
    __proto__: Uint16Array.__proto__,
    [Symbol.species]: Uint16Array,
};

var uint16 = array.slice();
shouldBe(uint16 instanceof Uint16Array, true);
shouldBe(uint16.length, 1024);
