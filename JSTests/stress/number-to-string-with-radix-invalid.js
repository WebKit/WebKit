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

function test(i, radix)
{
    return i.toString(radix);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    shouldThrow(() => test(i, 42), `RangeError: toString() radix argument must be between 2 and 36`);
}
