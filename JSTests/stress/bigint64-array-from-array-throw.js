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
    var a = new Int32Array([0, 1, 2, 3]);
    BigInt64Array.from(a);
}, `TypeError: Content types of source and destination typed arrays are different`);

shouldThrow(() => {
    var a = [0, 1, 2, 3];
    BigInt64Array.from(a);
}, `TypeError: Invalid argument type in ToBigInt operation`);

shouldThrow(() => {
    var a = [0.0, 0.1, 0.2, 0.3];
    BigInt64Array.from(a);
}, `TypeError: Invalid argument type in ToBigInt operation`);
