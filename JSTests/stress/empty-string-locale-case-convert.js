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
    'a'.toLocaleLowerCase('');
}, `RangeError: invalid language tag: `);
shouldThrow(() => {
    'a'.toLocaleUpperCase('');
}, `RangeError: invalid language tag: `);
shouldThrow(() => {
    ''.toLocaleLowerCase('');
}, `RangeError: invalid language tag: `);
shouldThrow(() => {
    ''.toLocaleUpperCase('');
}, `RangeError: invalid language tag: `);
