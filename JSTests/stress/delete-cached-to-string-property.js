function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {
    __proto__: {
        toString: function () {
            return "Hey";
        }
    }
};

shouldBe(object + 42, `Hey42`);
shouldBe(object + 42, `Hey42`);
delete object.__proto__.toString;
shouldBe(object + 42, `[object Object]42`);
