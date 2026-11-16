//@ requireOptions("--useWasmFaultSignalHandler=false")

// ref.cast of a null reference must trap even when the fault signal handler is
// unavailable. The JIT is allowed to skip the explicit null check only because the
// cast dereferences the reference and the handler turns the resulting fault into a
// trap; with no handler installed the check has to be emitted, or this crashes.
//
// (module
//   (type $s (struct (field (mut i32))))
//   (func (export "f") (param (ref null $s)) (result (ref $s))
//     local.get 0
//     ref.cast (ref $s)))

import * as assert from "../assert.js";

const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x0c, 0x02, 0x5f, 0x01, 0x7f, 0x01, 0x60, 0x01, 0x63, 0x00, 0x01, 0x64, 0x00,
    0x03, 0x02, 0x01, 0x01,
    0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0xfb, 0x16, 0x00, 0x0b,
]);

const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));

// The reported trap kind differs between tiers (CastFailure vs NullAccess), so only
// require that a trap is what comes out.
for (let i = 0; i < 2000; ++i)
    assert.throws(() => instance.exports.f(null), WebAssembly.RuntimeError, "");
