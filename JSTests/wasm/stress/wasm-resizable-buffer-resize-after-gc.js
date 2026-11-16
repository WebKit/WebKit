// Resizing a Wasm-backed resizable ArrayBuffer should work
// even after the originating WebAssembly.Memory is GC'd.

function flushStackRoots(n) {
    let a = [n, {}, n + 1, {}, {}];
    if (n)
        return flushStackRoots(n - 1);
    return a;
}

for (let iter = 0; iter < 5; ++iter) {
    let buf;
    (function () {
        let m = new WebAssembly.Memory({ initial: 1, maximum: 100 });
        buf = m.toResizableBuffer();
    })();
    flushStackRoots(64);
    fullGC();
    flushStackRoots(64);
    fullGC();

    if (buf.byteLength !== 65536)
        throw new Error("bad initial length: " + buf.byteLength);
    if (!buf.resizable)
        throw new Error("not resizable");

    buf.resize(65536 * 100);

    if (buf.byteLength !== 65536 * 100)
        throw new Error("bad resized length: " + buf.byteLength);

    let view = new Uint8Array(buf);
    for (let i = 0; i < view.length; i += 4096) {
        if (view[i] !== 0)
            throw new Error("non-zero at " + i);
    }
    view[view.length - 1] = 0x42;
    if (view[view.length - 1] !== 0x42)
        throw new Error("write failed");
}
