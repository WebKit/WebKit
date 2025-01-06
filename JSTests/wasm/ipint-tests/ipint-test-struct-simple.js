import * as assert from "../assert.js"

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

/*
(module
    (type $point (struct (field $x i32) (field $y i32)))
    (func (export "default_init") (result i32)
        (struct.new_default $point)
        (struct.get $point 0)
    )
)
 */

function test() {
    const instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8b\x80\x80\x80\x00\x02\x5f\x02\x7f\x00\x7f\x00\x60\x00\x01\x7f\x03\x82\x80\x80\x80\x00\x01\x01\x07\x90\x80\x80\x80\x00\x01\x0c\x64\x65\x66\x61\x75\x6c\x74\x5f\x69\x6e\x69\x74\x00\x00\x0a\x8f\x80\x80\x80\x00\x01\x89\x80\x80\x80\x00\x00\xfb\x01\x00\xfb\x02\x00\x00\x0b"));
    const { default_init } = instance.exports
    assert.eq(default_init(), 0)
}

test()

