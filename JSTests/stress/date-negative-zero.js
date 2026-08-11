function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var date = new Date(-0);
shouldBe(Object.is(date.getTime(), 0), true);
