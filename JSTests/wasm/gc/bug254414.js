//@ skip if !$isSIMDPlatform
//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

// (module
//   (type $0 (sub (func (param i32 i32 i32) (result i32))))
//   (type $1 (func (param (ref null $0))))
//   (type $2 (func (param i64 v128 v128 v128)))
//   (type $3 (func))
//   (memory $0 16 32)
//   (table $0 1 3 funcref)
//   (elem $0 (i32.const 0) $0)
//   (tag $tag$0 (param (ref null $0)))
//   (tag $tag$1 (param i64 v128 v128 v128))
//   (tag $tag$2)
//   (export "main" (func $0))
//   (func $0 (type $0) (param $0 i32) (param $1 i32) (param $2 i32) (result i32)
//    (try $label$3 (result i32)
//     (do
//      (drop
//       (i32.const 0)
//      )
//      (drop
//       (f64.const 0)
//      )
//      (i32.store offset=20746
//       (i32.const 0)
//       (i32.const 0)
//      )
//      (throw $tag$2)
//     )
//     (catch $tag$0
//      (drop
//       (pop (ref null $0))
//      )
//      (i32.const 0)
//     )
//     (catch_all
//      (i32.const 0)
//     )
//    )
//   )
//  )
const m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x9d\x80\x80\x80\x00\x05\x50\x00\x5f\x00\x50\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x60\x01\x63\x01\x00\x60\x04\x7e\x7b\x7b\x7b\x00\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x01\x04\x85\x80\x80\x80\x00\x01\x70\x01\x01\x03\x05\x84\x80\x80\x80\x00\x01\x01\x10\x20\x0d\x87\x80\x80\x80\x00\x03\x00\x02\x00\x03\x00\x04\x07\x88\x80\x80\x80\x00\x01\x04\x6d\x61\x69\x6e\x00\x00\x09\x8b\x80\x80\x80\x00\x01\x06\x00\x41\x00\x0b\x70\x01\xd2\x00\x0b\x0a\xb0\x80\x80\x80\x00\x01\x2e\x00\x06\x7f\x41\x00\x44\x00\x00\x00\x00\x00\x00\x00\x00\x41\x00\x41\x00\x36\x02\x8a\xa2\x01\x08\x02\x9b\xaa\x45\xfe\x2c\x02\xb2\xe4\x94\x88\x05\x07\x00\x1a\x41\x00\x19\x41\x00\x0b\x0b"));
m.exports.main();
