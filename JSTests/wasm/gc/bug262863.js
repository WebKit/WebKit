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
 *  (type $0 (sub (array (mut i32))))
 *  (type $1 (func (param (ref null $0))))
 *  (type $2 (sub (func (param i32 i32 i32) (result i32))))
 *  (memory $0 16 32)
 *  (table $0 1 1 funcref)
 *  (elem $0 (i32.const 0) $0)
 *  (tag $tag$0 (param (ref null $0)))
 *  (export "main" (func $0))
 *  (func $0 (param $0 i32) (param $1 i32) (param $2 i32) (result i32)
 *   (throw $tag$0
 *    (block $label$1 (result (ref null $0))
 *     (block $label$2 (result (ref null $0))
 *      (ref.null none)
 *     )
 *    )
 *   )
 *  )
 * )
 */
new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x9c\x80\x80\x80\x00\x05\x50\x00\x5f\x00\x50\x00\x5f\x00\x50\x00\x5e\x7f\x01\x50\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x60\x01\x63\x02\x00\x03\x82\x80\x80\x80\x00\x01\x03\x04\x85\x80\x80\x80\x00\x01\x70\x01\x01\x01\x05\x84\x80\x80\x80\x00\x01\x01\x10\x20\x0d\x83\x80\x80\x80\x00\x01\x00\x04\x07\x88\x80\x80\x80\x00\x01\x04\x6d\x61\x69\x6e\x00\x00\x09\x8b\x80\x80\x80\x00\x01\x06\x00\x41\x00\x0b\x70\x01\xd2\x00\x0b\x0a\x96\x80\x80\x80\x00\x01\x14\x00\x02\x63\x02\x02\x63\x02\xd0\x02\x0b\x0b\x08\x00\x41\xec\xa1\x98\xe3\x7d\x0b"));
