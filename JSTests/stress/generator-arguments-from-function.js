function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

function *gen(a, b, c)
{
    function test()
    {
        return arguments;
    }

    return test;
}

let g = gen(1, 2, 3);
let {value: func} = g.next();
shouldBe(func().length, 0);
