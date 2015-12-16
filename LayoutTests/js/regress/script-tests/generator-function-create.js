function createGeneratorFunction()
{
    function *gen()
    {
        yield 20;
        yield 30;
    }
    return gen;
}
function test()
{
    for (var i = 0; i < 500; ++i)
        createGeneratorFunction();
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
