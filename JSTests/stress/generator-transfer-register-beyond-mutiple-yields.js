function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


function *gen()
{
    var test = 42;
    yield 32;
    yield 33;
    yield test;
}

var g = gen();
shouldBe(g.next().value, 32);
shouldBe(g.next().value, 33);
shouldBe(g.next().value, 42);
