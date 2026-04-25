function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var AsyncFunctionPrototype = async function() {}.__proto__;

function testAsyncFunctionExpression()
{
    return async function()
    {
        await 42;
        return 1;
    };
}
noInline(testAsyncFunctionExpression);

function testAsyncFunctionDeclaration()
{
    async function asyncFn()
    {
        await 42;
        return 1;
    }

    return asyncFn;
}
noInline(testAsyncFunctionDeclaration);

function testAsyncArrowFunction()
{
    return async() =>
    {
        await 42;
        return 1;
    };
}
noInline(testAsyncArrowFunction);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(testAsyncFunctionExpression().__proto__, AsyncFunctionPrototype);
    shouldBe(testAsyncFunctionDeclaration().__proto__, AsyncFunctionPrototype);
    shouldBe(testAsyncArrowFunction().__proto__, AsyncFunctionPrototype);
}
