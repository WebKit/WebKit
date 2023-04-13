function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var flag = false;
function mayExit()
{
    if (flag) {
        OSRExit();
        return 24;
    }
    return 42;
}

function inlined(a, b, c)
{
    return a + b + c + mayExit();
}

var bound = inlined.bind(undefined, 1, 2, 3);

function test()
{
    return bound();
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test(), 48);
flag = true;
shouldBe(test(), 30);
