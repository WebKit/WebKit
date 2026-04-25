function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe([].join(''), '');
shouldBe(['', ''].join(''), '');
shouldBe(['a', 'b'].join(''), 'ab');
