var exception;

try {
    new WebAssembly.Memory({ initial: 0x10001, maximum: 0x10001 }).buffer;
} catch (e) {
    exception = e;
}

if (exception != "RangeError: WebAssembly.Memory 'initial' page count is too large")
    throw "FAILED, exception was: " + exception;
