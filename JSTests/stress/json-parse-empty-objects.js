function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var string = '[{},{},{},{}]';
shouldBe(JSON.stringify(JSON.parse(string)), `[{},{},{},{}]`);

var string = '[[],[],[],[]]';
shouldBe(JSON.stringify(JSON.parse(string)), `[[],[],[],[]]`);
