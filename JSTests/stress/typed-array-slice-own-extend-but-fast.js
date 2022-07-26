function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = new Uint8Array(1024);
var uint8 = array.slice();
shouldBe(uint8 instanceof Uint8Array, true);
shouldBe(uint8.length, 1024);
uint8.hello = "Hello";
var uint8_2 = array.slice();
shouldBe(uint8_2 instanceof Uint8Array, true);
shouldBe(uint8_2.length, 1024);
