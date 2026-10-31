// (module
//   (import "env" "suspending" (func $suspending (result i32)))
//   (func (export "f") (result i32) (call $suspending)))
const A = new Uint8Array([0,97,115,109,1,0,0,0,1,5,1,96,0,1,127,2,18,1,3,101,110,118,10,115,117,115,112,101,110,100,105,110,103,0,0,3,2,1,0,7,5,1,1,102,0,1,10,6,1,4,0,16,0,11]);

// (module
//   (import "a" "f" (func $f (result i32)))
//   (func (export "g") (result i32) (return_call $f)))
const B = new Uint8Array([0,97,115,109,1,0,0,0,1,5,1,96,0,1,127,2,7,1,1,97,1,102,0,0,3,2,1,0,7,5,1,1,103,0,1,10,6,1,4,0,18,0,11]);

const suspending = new WebAssembly.Suspending(() => 42);
const a = new WebAssembly.Instance(new WebAssembly.Module(A), { env: { suspending } });
const b = new WebAssembly.Instance(new WebAssembly.Module(B), { a: a.exports });

// This should not raise.
await WebAssembly.promising(b.exports.g)();
