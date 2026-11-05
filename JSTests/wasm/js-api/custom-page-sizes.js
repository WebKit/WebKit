//@ requireOptions("--useWasmCustomPageSizes=1")
import * as assert from "../assert.js";

function memoryModule(flags, initial, extra = []) {
    const payload = [0x01, flags, initial, ...extra];
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x05, payload.length, ...payload,
    ]);
}

function compiles(bytes) {
    try {
        new WebAssembly.Module(bytes);
        return true;
    } catch (e) {
        return false;
    }
}

function throws(func, type) {
    try {
        func();
    } catch (e) {
        if (e instanceof type)
            return;
        throw new Error("expected " + type.name + ", got " + e);
    }
    throw new Error("expected " + type.name);
}

{
    const oneByte = memoryModule(0x08, 0x01, [0x00]);
    assert.truthy(compiles(oneByte));
    throws(() => new WebAssembly.Instance(new WebAssembly.Module(oneByte)), RangeError);
}

{
    const explicitDefault = memoryModule(0x08, 0x01, [0x10]);
    new WebAssembly.Instance(new WebAssembly.Module(explicitDefault));
}

{
    assert.falsy(compiles(memoryModule(0x08, 0x01, [0x01])));
    assert.falsy(compiles(memoryModule(0x08, 0x01, [0x40])));
}

{
    const plain = memoryModule(0x00, 0x01);
    new WebAssembly.Instance(new WebAssembly.Module(plain));
}
