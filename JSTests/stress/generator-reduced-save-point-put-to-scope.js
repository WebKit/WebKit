function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function error()
{
    throw "ok";
}

function* gen()
{
    var value = 42;
    try {
        yield 300;
        value = 500;
        error();
    } catch (e) {
        yield 42;
        return value;
    }
    return 200;
}

var g = gen();
shouldBe(g.next().value, 300);
shouldBe(g.next().value, 42);
shouldBe(g.next().value, 500);
