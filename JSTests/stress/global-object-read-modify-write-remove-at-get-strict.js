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

shouldThrow(() => {
    runString(`
    'use strict';
    Object.defineProperty(globalThis, 'x', {
        get() {
            delete globalThis.x;
            return 2;
        },
        configurable: true,
    });

    x += 42;
    `);
}, `ReferenceError: Can't find variable: x`);
