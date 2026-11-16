//@ memoryHog!
//@ skip if $addressBits <= 32
//@ runDefault

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`bad error: ${String(error)}`);
}

function spread(iterable) {
    return [...iterable];
}

shouldThrow(() => spread(new Uint8Array(2 ** 32 + 3)), RangeError);
