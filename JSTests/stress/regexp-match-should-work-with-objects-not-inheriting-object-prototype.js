function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}
var regexp = Object.create(null);
regexp.reg = /Hello/;
regexp.exec = function (value) {
    return regexp.reg.exec(value);
};
var string = "Hello";
shouldBe(RegExp.prototype[Symbol.match].call(regexp, string)[0], string)
