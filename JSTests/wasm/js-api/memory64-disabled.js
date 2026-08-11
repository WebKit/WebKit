//@ requireOptions("--useWasmMemory64=false")
import * as assert from "../assert.js";

// With Memory64 disabled, the JS API must not hand out i64 memories or tables either. Otherwise a
// feature check succeeds and the module that would consume it fails to compile.

assert.throws(() => new WebAssembly.Memory({ initial: 1n, address: "i64" }), TypeError,
    "WebAssembly.Memory 'address' of 'i64' requires Memory64 to be enabled");
assert.throws(() => new WebAssembly.Table({ initial: 1n, element: "externref", address: "i64" }), TypeError,
    "WebAssembly.Table 'address' of 'i64' requires Memory64 to be enabled");

// i32 memories and tables are unaffected, including an explicit address.
new WebAssembly.Memory({ initial: 1 });
new WebAssembly.Memory({ initial: 1, address: "i32" });
new WebAssembly.Table({ initial: 1, element: "externref" });
new WebAssembly.Table({ initial: 1, element: "externref", address: "i32" });

// A module declaring an i64 memory or table is still rejected.
function declaresI64Memory() {
    // (module (memory i64 1)) -- limits flags 0x04 selects the i64 index type.
    return new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x05, 0x03, 0x01, 0x04, 0x01]);
}
assert.throws(() => new WebAssembly.Module(declaresI64Memory()), WebAssembly.CompileError, "Memory64 is not enabled");
