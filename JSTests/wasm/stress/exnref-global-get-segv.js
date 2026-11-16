// https://bugs.webkit.org/show_bug.cgi?id=293340
// Importing an exported immutable (ref null exn) global must succeed.
// Global::get() rejects exnref for the JS API; linking must copy the stored ref.

import * as assert from "../assert.js";

// (module
//   (global (export "g") (ref null exn) (ref.null exn))
// )
const wasm1 = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 6, 6, 1, 105, 0, 208, 105, 11, 7, 5, 1, 1, 103, 3, 0]);
const module1 = new WebAssembly.Module(wasm1);
const instance1 = new WebAssembly.Instance(module1);

// (module
//   (import "M" "g" (global (ref null exn)))
//   (func (export "is_null") (result i32)
//     global.get 0
//     ref.is_null)
// )
const wasm2 = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
    0x02, 0x08, 0x01, 0x01, 0x4d, 0x01, 0x67, 0x03, 0x69, 0x00,
    0x03, 0x02, 0x01, 0x00,
    0x07, 0x0b, 0x01, 0x07, 0x69, 0x73, 0x5f, 0x6e, 0x75, 0x6c, 0x6c, 0x00, 0x00,
    0x0a, 0x07, 0x01, 0x05, 0x00, 0x23, 0x00, 0xd1, 0x0b,
]);
const module2 = new WebAssembly.Module(wasm2);
const instance2 = new WebAssembly.Instance(module2, { M: { g: instance1.exports.g } });
assert.eq(instance2.exports.is_null(), 1);
