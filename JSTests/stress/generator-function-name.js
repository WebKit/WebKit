function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var ok = function *generator()
{
    yield generator;
};

var g = ok();
shouldBe(g.next().value, ok);
