function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {};
object.__proto__ = {
    toString: undefined,
    valueOf: function () {
        return 42;
    }
};
shouldBe(object + 42, 84);
shouldBe(object + 42, 84);
object.__proto__.valueOf = function () {
    return 43;
};
shouldBe(object + 42, 85);
