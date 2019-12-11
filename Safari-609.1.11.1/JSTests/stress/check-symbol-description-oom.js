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

const s0 = (-1).toLocaleString();
const s1 = s0.padEnd(2147483647, '  ');
shouldThrow(() => Symbol(s1), `Error: Out of memory`);
