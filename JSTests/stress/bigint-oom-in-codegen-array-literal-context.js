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

const largeNumber = 0xffffff;
let generated = null;
function test()
{
    generated = new Function(`target`, `return [0x${'f'.repeat(largeNumber)}n]`);
}

test();
shouldThrow(() => {
    generated();
}, `RangeError: Out of memory`);
