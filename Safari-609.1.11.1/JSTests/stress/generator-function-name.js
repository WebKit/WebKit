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

function* generator2(factory)
{
    shouldBe(generator2, factory);
};

generator2(generator2).next();
