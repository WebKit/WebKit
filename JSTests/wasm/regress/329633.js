// (module
//   (import "env" "suspending" (func $suspending (result i32)))
//   (memory 1)
//   (func (export "f") (result i32) (call $suspending)))
const A = new Uint8Array([0,97,115,109,1,0,0,0,1,5,1,96,0,1,127,2,18,1,3,101,110,118,10,115,117,115,112,101,110,100,105,110,103,0,0,3,2,1,0,5,3,1,0,1,7,5,1,1,102,0,1,10,6,1,4,0,16,0,11]);

// (module
//   (import "a" "f" (func $f (result i32)))
//   (memory 1)
//   (data (i32.const 0) "\04")
//   (func $tail (result i32) (return_call $f))
//   (func (export "g") (result i32)
//     (i32.add (call $tail) (i32.load (i32.const 0)))))
const B = new Uint8Array([0,97,115,109,1,0,0,0,1,5,1,96,0,1,127,2,7,1,1,97,1,102,0,0,3,3,2,0,0,5,3,1,0,1,7,5,1,1,103,0,2,10,17,2,4,0,18,0,11,10,0,16,1,65,0,40,2,0,106,11,11,7,1,0,65,0,11,1,4]);

const suspending = new WebAssembly.Suspending(async () => 42);
const a = new WebAssembly.Instance(new WebAssembly.Module(A), { env: { suspending } });
const b = new WebAssembly.Instance(new WebAssembly.Module(B), { a: a.exports });

// This should not raise.
const res = await WebAssembly.promising(b.exports.g)();
if (res !== 46)
    throw new Error("expected 46, got ${result}");
