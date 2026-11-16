//@ memoryHog!
//@ runDefault

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

var hugeFlags = "a".repeat(2 ** 31 - 1);

shouldThrow(() => {
    var fakeRegExp = {
        get flags() { return hugeFlags; },
    };
    RegExp.prototype[Symbol.split].call(fakeRegExp, "abc");
}, "RangeError: Out of memory");
