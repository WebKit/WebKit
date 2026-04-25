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

let i64a = new BigInt64Array(new SharedArrayBuffer(128));
let u64a = new BigUint64Array(new SharedArrayBuffer(128));

shouldThrow(() => {
    Atomics.wait(u64a, 0, 0n);
}, `TypeError: Typed array argument must be an Int32Array or BigInt64Array.`);

shouldThrow(() => {
    Atomics.waitAsync(u64a, 0, 0n);
}, `TypeError: Typed array argument must be an Int32Array or BigInt64Array.`);

shouldThrow(() => {
    Atomics.load(new Float64Array(8), 0);
}, `TypeError: Typed array argument must be an Int8Array, Int16Array, Int32Array, Uint8Array, Uint16Array, Uint32Array, BigInt64Array, or BigUint64Array.`);
