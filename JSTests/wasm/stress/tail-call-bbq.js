$262.agent.start(String.raw`
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
 *  (type $0 (func))
 *  (type $1 (sub (func (param i32 i32 i32) (result i32))))
 *  (type $2 (sub (array (mut i32))))
 *  (type $3 (sub (struct )))
 *  (memory $0 16 32)
 *  (table $0 1 1 funcref)
 *  (elem $0 (i32.const 0) $0)
 *  (tag $tag$0)
 *  (export "main" (func $0))
 *  (func $0 (param $0 i32) (param $1 i32) (param $2 i32) (result i32)
 *   (local $3 (ref null $2))
 *   (local $4 i64)
 *   (local $5 (ref null $3))
 *   (return_call $0
 *    (i32.const 102)
 *    (i32.const -94)
 *    (i32.const -36)
 *   )
 *  )
 * )
 *
 */
const m = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x96\x80\x80\x80\x00\x04\x50\x00\x5f\x00\x50\x00\x5e\x7f\x01\x50\x00\x60\x03\x7f\x7f\x7f\x01\x7f\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x02\x04\x85\x80\x80\x80\x00\x01\x70\x01\x01\x01\x05\x84\x80\x80\x80\x00\x01\x01\x10\x20\x0d\x83\x80\x80\x80\x00\x01\x00\x03\x07\x88\x80\x80\x80\x00\x01\x04\x6d\x61\x69\x6e\x00\x00\x09\x8b\x80\x80\x80\x00\x01\x06\x00\x41\x00\x0b\x70\x01\xd2\x00\x0b\x0a\x97\x80\x80\x80\x00\x01\x15\x03\x01\x63\x01\x01\x7e\x01\x63\x00\x41\xe6\x00\x41\xa2\x7f\x41\x5c\x12\x00\xa8\x0b"));
m.exports.main();
`);
$262.agent.sleep(100);
