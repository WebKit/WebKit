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
 * (module
 *   (tag ...) ;; also has a tag section but GC spec decoder could not read it.
 *   (type $0 (sub (struct (field f64) (field i64) (field i64) (field v128))))
 *   (type $1 (sub (func (param i32 i32 i32) (result i32))))
 *   (type $2 (func))
 *   (type $3
 *     (sub (func (param v128 (ref 0) i32 i32 i32 i32 i32 i32 i32) (result i32)))
 *   )
 *   (table $0 1 8 funcref (ref.null func))
 *   (table $1 0 0 (ref null extern) (ref.null extern))
 *   (memory $0 16 32)
 *   (func $0
 *     (type 1)
 *     (i32.const 0)
 *     (i8x16.splat)
 *     (f64.const 0)
 *     (i64.const 0)
 *     (i64.const 0)
 *     (i32.const 0)
 *     (i8x16.splat)
 *     (struct.new 0)
 *     (i32.const 0)
 *     (i32.const 0)
 *     (i32.const 0)
 *     (i32.const 0)
 *     (i32.const 0)
 *     (i32.const 0)
 *     (i32.const 0)
 *     (block
 *       (type 3)
 *       (drop)
 *       (drop)
 *       (drop)
 *       (drop)
 *       (drop)
 *       (drop)
 *       (drop)
 *       (drop)
 *       (drop)
 *       (i32.const 0)
 *     )
 *   )
 *   (export "main" (func 0))
 *   (elem $0 (i32.const 0) funcref (ref.func 0))
 * )
 */
const m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\xa9\x80\x80\x80\x00\x04\x50\x00\x5f\x04\x7c\x00\x7e\x00\x7e\x00\x7b\x00\x50\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x60\x00\x00\x50\x00\x60\x09\x7b\x64\x00\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x01\x7f\x03\x82\x80\x80\x80\x00\x01\x01\x04\x89\x80\x80\x80\x00\x02\x70\x01\x01\x08\x6f\x01\x00\x00\x05\x84\x80\x80\x80\x00\x01\x01\x10\x20\x0d\x83\x80\x80\x80\x00\x01\x00\x02\x07\x88\x80\x80\x80\x00\x01\x04\x6d\x61\x69\x6e\x00\x00\x09\x8b\x80\x80\x80\x00\x01\x06\x00\x41\x00\x0b\x70\x01\xd2\x00\x0b\x0a\xb8\x80\x80\x80\x00\x01\x36\x00\x41\x00\xfd\x0f\x44\x00\x00\x00\x00\x00\x00\x00\x00\x42\x00\x42\x00\x41\x00\xfd\x0f\xfb\x00\x00\x41\x00\x41\x00\x41\x00\x41\x00\x41\x00\x41\x00\x41\x00\x02\x03\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x41\x00\x0b\x0b"));
m.exports.main();
