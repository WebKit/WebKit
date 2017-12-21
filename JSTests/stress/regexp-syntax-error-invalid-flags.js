function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

function test()
{
    return /Hello/cocoa;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    shouldThrow(test, `SyntaxError: Invalid regular expression: invalid flags`);
