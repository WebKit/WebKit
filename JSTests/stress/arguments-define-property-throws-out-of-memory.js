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

function foo() {
    Object.defineProperty(arguments, '0', {value: s});
}

let s = [0.1].toLocaleString().padEnd(2 ** 31 - 1, 'ab');

shouldThrow(() => {
    foo(s);
}, `RangeError: Out of memory`);
