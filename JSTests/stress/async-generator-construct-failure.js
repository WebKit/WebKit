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

async function* gen() { }

shouldThrow(() => {
    new gen()
}, `TypeError: function is not a constructor (evaluating 'new gen()')`);

shouldThrow(() => {
    Reflect.construct(gen, [], {});
}, `TypeError: Reflect.construct requires the first argument be a constructor`);
