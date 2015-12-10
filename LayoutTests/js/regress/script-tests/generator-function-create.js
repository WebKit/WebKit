function createGeneratorFunction()
{
    function *gen()
    {
        yield 20;
        yield 30;
    }
    return gen;
}
noInline(createGeneratorFunction);
for (var i = 0; i < 1e4; ++i)
    createGeneratorFunction();
