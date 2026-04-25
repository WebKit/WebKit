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

function test(value)
{
    return Object.create(value);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    shouldThrow(() => {
        test(undefined);
    }, `TypeError: Object prototype may only be an Object or null.`);
}
for (var i = 0; i < 1e4; ++i) {
    // Some folding does not happen in the non-inlined version, so this can test a different path through the compiler
    // than the previous loop.
    shouldThrow(() => {
        Object.create(undefined);
    }, `TypeError: Object prototype may only be an Object or null.`);
}
