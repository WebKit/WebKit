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
 *   (type $0 (sub (struct)))
 *   (type $1 (sub (array (mut i32))))
 *   (type $2 (sub (func (param i32 i32 i32) (result i32))))
 *   (type $3 (func))
 *   (table $0 1 25 funcref (ref.null func))
 *   (table $1 0 0 (ref i31) (i32.const 0) (ref.i31))
 *   (tag ...) ;; binary also contains a tag section, GC decoder couldn't parse it
 *   (memory $0 16 32)
 *   (func $0 (type 2) (i32.const 434_800_715))
 *   (export "main" (func 0))
 *   (elem $0 (i32.const 0) funcref (ref.func 0))
 * )
 *
 */
const m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x96\x80\x80\x80\x00\x04\x50\x00\x5f\x00\x50\x00\x5e\x7f\x01\x50\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x02\x04\x96\x80\x80\x80\x00\x02\x40\x00\x70\x01\x01\x19\xd0\x70\x0b\x40\x00\x64\x6c\x01\x00\x00\x41\x00\xfb\x1c\x0b\x05\x84\x80\x80\x80\x00\x01\x01\x10\x20\x0d\x85\x80\x80\x80\x00\x02\x00\x03\x00\x03\x07\x88\x80\x80\x80\x00\x01\x04\x6d\x61\x69\x6e\x00\x00\x09\x8b\x80\x80\x80\x00\x01\x06\x00\x41\x00\x0b\x70\x01\xd2\x00\x0b\x0a\x8a\x80\x80\x80\x00\x01\x08\x00\x41\xcb\x90\xaa\xcf\x01\x0b"));
m.exports.main();
