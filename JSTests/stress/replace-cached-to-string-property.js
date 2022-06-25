function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {};
object.__proto__ = {
    toString: function () {
        return "Hi";
    }
};
shouldBe(object + 42, `Hi42`);
shouldBe(object + 42, `Hi42`);
object.__proto__.toString = function () {
    return "Hey";
};
shouldBe(object + 42, `Hey42`);
