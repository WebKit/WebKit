function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [null, null, null];
shouldBe(array.toLocaleString(), `,,`)
