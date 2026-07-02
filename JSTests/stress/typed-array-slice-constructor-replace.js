function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = new Uint8Array(1024);
var uint8 = array.slice();
shouldBe(uint8 instanceof Uint8Array, true);
shouldBe(uint8.length, 1024);
array.constructor = Uint16Array;
var uint16 = array.slice();
shouldBe(uint16 instanceof Uint16Array, true);
shouldBe(uint16.length, 1024);
