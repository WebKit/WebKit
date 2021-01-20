function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(typeof Reflect, "object");
shouldBe(Reflect, Reflect);
shouldBe(Object.getPrototypeOf(Reflect), Object.getPrototypeOf({}));
shouldBe(Reflect.toString(), "[object Reflect]");
shouldBe(Reflect.hasOwnProperty, Object.prototype.hasOwnProperty);
