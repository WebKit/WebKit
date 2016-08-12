function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


function *g() { }
var GeneratorFunctionPrototype = g.__proto__;

function test()
{
    return function *gen()
    {
        yield 42;
    };
}
noInline(test);

function test2()
{
    function *gen()
    {
        yield 42;
    }

    return gen;
}
noInline(test2);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(test().__proto__, GeneratorFunctionPrototype);
    shouldBe(test2().__proto__, GeneratorFunctionPrototype);
}
