function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = new Uint8Array(128);
Reflect.defineProperty(array, 'length', {
    value: 42
});
var result = Uint8Array.from(array);
shouldBe(result.length, 128); // Ignore "length"
