function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(RegExp.prototype.hasOwnProperty('exec'), true);
shouldBe(delete RegExp.prototype.exec, true);
shouldBe(RegExp.prototype.hasOwnProperty('exec'), false);
shouldBe(delete RegExp.prototype.exec, true);
shouldBe(RegExp.prototype.hasOwnProperty('exec'), false);
