//@ skip if $architecture != "arm64" && $architecture != "x86-64"

var exception;
try {
    var memory = new WebAssembly.Memory({
        initial: 65536
    });
    new Uint8Array(memory.buffer).toString();
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED: " + exception;
