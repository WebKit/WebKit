//@ skip unless $isSIMDPlatform
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
 * Output from wasm-dis, looks to have elided some code such as struct defs (and v128 use):
 *
 * (module
 *  (type $0 (sub (func)))
 *  (type $1 (func))
 *  (type $2 (sub (func (param i32 i32 i32) (result i32))))
 *  (memory $0 16 32)
 *  (table $0 4 4 funcref)
 *  (elem $0 (i32.const 0) $0 $1 $2 $3)
 *  (tag $tag$0)
 *  (export "main" (func $0))
 *  (func $0 (type $2) (param $0 i32) (param $1 i32) (param $2 i32) (result i32)
 *   (i32.const 1060103818)
 *  )
 *  (func $1 (type $0)
 *  )
 *  (func $2 (type $0)
 *  )
 *  (func $3 (type $0)
 *  )
 * )
 */
const m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\xad\x80\x80\x80\x00\x07\x50\x00\x5f\x03\x7b\x00\x7f\x00\x7f\x00\x50\x00\x5e\x7f\x01\x50\x00\x5e\x7f\x01\x50\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x50\x00\x60\x00\x00\x50\x00\x60\x00\x00\x50\x00\x60\x00\x00\x03\x85\x80\x80\x80\x00\x04\x03\x04\x05\x06\x04\x85\x80\x80\x80\x00\x01\x70\x01\x04\x04\x05\x84\x80\x80\x80\x00\x01\x01\x10\x20\x0d\x83\x80\x80\x80\x00\x01\x00\x04\x07\x88\x80\x80\x80\x00\x01\x04\x6d\x61\x69\x6e\x00\x00\x09\x94\x80\x80\x80\x00\x01\x06\x00\x41\x00\x0b\x70\x04\xd2\x00\x0b\xd2\x01\x0b\xd2\x02\x0b\xd2\x03\x0b\x0a\x93\x80\x80\x80\x00\x04\x08\x00\x41\x8a\xcd\xbf\xf9\x03\x0b\x02\x00\x0b\x02\x00\x0b\x02\x00\x0b"));
m.exports.main();
