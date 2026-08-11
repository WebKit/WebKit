//@ requireOptions("--useWasmMultiMemory=1")

import * as assert from "../assert.js";

// Test that active data segments target the correct memory in a multi-memory
// module. The bundled WABT doesn't support multi-memory data segment syntax,
// so we construct the binary directly.
//
// Equivalent WAT:
//   (module
//     (memory 1)
//     (memory 1)
//     (func (export "load_m0") (param i32) (result i64) (local.get 0) (i64.load (memory 0)))
//     (func (export "load_m1") (param i32) (result i64) (local.get 0) (i64.load (memory 1)))
//     (data (memory 0) (i32.const 0) "\01")
//     (data (memory 1) (i32.const 0) "\02")
//   )

const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // type section: one type (i32) -> (i64)
    0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7e,

    // function section: two functions, both type 0
    0x03, 0x03, 0x02, 0x00, 0x00,

    // memory section: two memories, each 1 page
    0x05, 0x05, 0x02, 0x00, 0x01, 0x00, 0x01,

    // export section: "load_m0" -> func 0, "load_m1" -> func 1
    0x07, 0x15, 0x02,
    0x07, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x6d, 0x30, 0x00, 0x00,
    0x07, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x6d, 0x31, 0x00, 0x01,

    // code section: two function bodies
    0x0a, 0x12, 0x02,
    // func 0: local.get 0, i64.load align=3 offset=0 (memory 0)
    0x07, 0x00, 0x20, 0x00, 0x29, 0x03, 0x00, 0x0b,
    // func 1: local.get 0, i64.load align=3 offset=0 (memory 1, bit 6 set)
    0x08, 0x00, 0x20, 0x00, 0x29, 0x43, 0x01, 0x00, 0x0b,

    // data section: two active segments
    0x0b, 0x0e, 0x02,
    // segment 0: flag=0x00 (active, memory 0), offset=i32.const 0, 1 byte: 0x01
    0x00, 0x41, 0x00, 0x0b, 0x01, 0x01,
    // segment 1: flag=0x02 (active, explicit memory index), memory=1, offset=i32.const 0, 1 byte: 0x02
    0x02, 0x01, 0x41, 0x00, 0x0b, 0x01, 0x02,
]);

const module = new WebAssembly.Module(bytes.buffer);
const instance = new WebAssembly.Instance(module);

assert.eq(instance.exports.load_m0(0), 1n);
assert.eq(instance.exports.load_m1(0), 2n);
