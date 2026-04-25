function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var x = Object.getOwnPropertyDescriptor(Array, Symbol.species).get;
shouldBe(x(), undefined);
