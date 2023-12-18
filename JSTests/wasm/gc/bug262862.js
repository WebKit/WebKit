//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true", "--useWebAssemblyGC=true")

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

/*
 *
 * (module
 *  (type $0 (func (param i32)))
 *  (type $1 (sub (array (mut i32))))
 *  (type $2 (sub (func (param i32 i32 i32) (result i32))))
 *  (memory $0 16 32)
 *  (table $0 1 2 funcref)
 *  (table $1 31 (ref $1))
 *  (elem $0 (table $0) (i32.const 0) func $0)
 *  (tag $tag$0 (param i32))
 *  (export "main" (func $0))
 *  (func $0 (param $0 i32) (param $1 i32) (param $2 i32) (result i32)
 *   (local.get $0)
 *  )
 * )
 *
 */
const m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x99\x80\x80\x80\x00\x04\x50\x00\x5f\x01\x7f\x00\x50\x00\x5e\x7f\x01\x50\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x60\x01\x7f\x00\x03\x82\x80\x80\x80\x00\x01\x02\x04\x89\x80\x80\x80\x00\x02\x70\x01\x01\x02\x6d\x01\x00\x1f\x05\x84\x80\x80\x80\x00\x01\x01\x10\x20\x0d\x83\x80\x80\x80\x00\x01\x00\x03\x07\x88\x80\x80\x80\x00\x01\x04\x6d\x61\x69\x6e\x00\x00\x09\x8b\x80\x80\x80\x00\x01\x06\x00\x41\x00\x0b\x70\x01\xd2\x00\x0b\x0a\x86\x80\x80\x80\x00\x01\x04\x00\x20\x00\x0b"));
m.exports.main();
