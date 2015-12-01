function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function *gen()
{
    yield new.target;
}

var g = gen();
shouldBe(g.next().value, undefined);

var g2 = new gen();
shouldBe(g.next().value, undefined);
