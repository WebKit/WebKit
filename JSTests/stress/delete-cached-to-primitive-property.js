function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {
    __proto__: {
        [Symbol.toPrimitive]: function () {
            return 42;
        }
    }
};

shouldBe(object + 42, 84);
shouldBe(object + 42, 84);
delete object.__proto__[Symbol.toPrimitive];
shouldBe(object + 42, `[object Object]42`);
