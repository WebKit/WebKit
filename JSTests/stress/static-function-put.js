function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
var exec = RegExp.prototype.exec;
var nested = Object.create(RegExp.prototype);

nested.exec = "Hello";
shouldBe(nested.hasOwnProperty('exec'), true);
shouldBe(nested.exec, "Hello");
shouldBe(/hello/.exec, exec);
