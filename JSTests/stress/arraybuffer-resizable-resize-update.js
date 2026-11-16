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

let buffer = new ArrayBuffer(16, {maxByteLength: 16});
shouldThrow(() => {
    buffer.resize({
        valueOf() {
            $.detachArrayBuffer(buffer);
            return 0;
        }
    });
}, `TypeError: Receiver is detached`);
