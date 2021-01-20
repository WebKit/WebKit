function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var s = 'xxxxxxxxxxxxxxAxxxxxxxxxxxxxxxxxxxxA–';
var result = s.replace(/A/g, 'b');
shouldBe(result, 'xxxxxxxxxxxxxxbxxxxxxxxxxxxxxxxxxxxb–');
