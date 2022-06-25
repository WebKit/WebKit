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
    let a = class c {
        static get[(a=0x12345678, b=0x42424242)]()
        {
        }
    };
}, `ReferenceError: Cannot access uninitialized variable.`);
