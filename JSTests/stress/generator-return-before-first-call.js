function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeIteratorResult(actual, { value, done })
{
    shouldBe(actual.value, value);
    shouldBe(actual.done, done);
}

function unreachable()
{
    throw new Error("NG");
}

function *gen()
{
    unreachable();
}

var g = gen();
shouldBeIteratorResult(g.return("Hello"), { value: "Hello", done: true });
