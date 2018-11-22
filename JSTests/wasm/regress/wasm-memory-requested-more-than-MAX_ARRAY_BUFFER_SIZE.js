var exception;

try {
    new WebAssembly.Memory({ initial: 0x8000, maximum: 0x8000 }).buffer;
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";
