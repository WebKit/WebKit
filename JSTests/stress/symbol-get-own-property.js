function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var symbol = Symbol("Cocoa");
shouldBe(symbol[0], undefined);

// ToObject(symbol).
symbol[0] = "Hello";
shouldBe(symbol[0], undefined);

Symbol.prototype[30] = 42;
shouldBe(symbol[30], 42);
