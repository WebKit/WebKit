//@ runWebAssemblySuite("--useWebAssemblyTypedFunctionReferences=true")
import * as assert from "../assert.js";

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

async function testTypedFuncrefTable() {
  /*
   * (module
   *   (type (func (result i32)))
   *   (elem declare funcref (ref.func 0))
   *   (table (export "t") 10 (ref null 0))
   *   (func (type 0) (i32.const 42))
   *   (func (export "set") (param i32)
   *     (table.set (local.get 0) (ref.func 0)))
   * )
   */
  {
    const m = new WebAssembly.Instance(
      module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x01\x7f\x60\x01\x7f\x00\x03\x83\x80\x80\x80\x00\x02\x00\x01\x04\x85\x80\x80\x80\x00\x01\x63\x00\x00\x0a\x07\x8b\x80\x80\x80\x00\x02\x01\x74\x01\x00\x03\x73\x65\x74\x00\x01\x09\x87\x80\x80\x80\x00\x01\x07\x70\x01\xd2\x00\x0b\x0a\x97\x80\x80\x80\x00\x02\x84\x80\x80\x80\x00\x00\x41\x2a\x0b\x88\x80\x80\x80\x00\x00\x20\x00\xd2\x00\x26\x00\x0b")
    );
    m.exports.set(1);
    assert.eq(m.exports.t.get(1)(), 42);
    assert.eq(m.exports.t.get(0), null);
  }
}

assert.asyncTest(testTypedFuncrefTable());
