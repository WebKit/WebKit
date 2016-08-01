function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = Object.defineProperties({}, {
    nonEnumerable: {
        enumerable: false,
        value: 42
    }
});

var result = Object.assign({}, object);
shouldBe(result.nonEnumerable, undefined);
