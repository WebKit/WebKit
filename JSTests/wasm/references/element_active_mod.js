import * as assert from '../assert.js';

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

function basicTest() {
  /*
  (module
    (func $f (result i32)
      (i32.const 37)
    )
    (func $g (result i32)
      (i32.const 42)
    )
    (table $t1 10 funcref)
    (table $t2 20 funcref)
    (elem (i32.const 3) funcref (ref.func $g) (ref.null func) (ref.func $f) (ref.null func))
    (elem (table $t2) (i32.const 7) funcref (ref.func $f) (ref.null func) (ref.func $g))
    (func (export "get_tbl1") (param $idx i32) (result funcref)
      (table.get $t1 (local.get $idx))
    )
    (func (export "get_tbl2") (param $idx i32) (result funcref)
      (table.get $t2 (local.get $idx))
    )
  )
  */
  let instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x0a\x02\x60\x00\x01\x7f\x60\x01\x7f\x01\x70\x03\x05\x04\x00\x00\x01\x01\x04\x07\x02\x70\x00\x0a\x70\x00\x14\x07\x17\x02\x08\x67\x65\x74\x5f\x74\x62\x6c\x31\x00\x02\x08\x67\x65\x74\x5f\x74\x62\x6c\x32\x00\x03\x09\x22\x02\x04\x41\x03\x0b\x04\xd2\x01\x0b\xd0\x70\x0b\xd2\x00\x0b\xd0\x70\x0b\x06\x01\x41\x07\x0b\x70\x03\xd2\x00\x0b\xd0\x70\x0b\xd2\x01\x0b\x0a\x19\x04\x04\x00\x41\x25\x0b\x04\x00\x41\x2a\x0b\x06\x00\x20\x00\x25\x00\x0b\x06\x00\x20\x00\x25\x01\x0b"));

  assert.eq(instance.exports.get_tbl1(3)(), 42);
  assert.eq(instance.exports.get_tbl1(4), null);
  assert.eq(instance.exports.get_tbl1(5)(), 37);
  assert.eq(instance.exports.get_tbl1(6), null);

  assert.eq(instance.exports.get_tbl2(7)(), 37);
  assert.eq(instance.exports.get_tbl2(8), null);
  assert.eq(instance.exports.get_tbl2(9)(), 42);
}

function refNullExternInElemsSection() {
  /*
  (module
    (table $t 10 funcref)
    (elem (i32.const 3) funcref (ref.null extern))
  )
  */
  assert.throws(() => module("\x00\x61\x73\x6d\x01\x00\x00\x00\x04\x04\x01\x70\x00\x0a\x09\x09\x01\x04\x41\x03\x0b\x01\xd0\x6f\x0b"),
  WebAssembly.CompileError,
  "WebAssembly.Module doesn't parse at byte 25: Element section's 0th element's init_expr opcode of type Externref doesn't match element's type Funcref");
}

basicTest();
refNullExternInElemsSection();
