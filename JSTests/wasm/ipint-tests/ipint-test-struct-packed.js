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
    (type $point (struct (field $x i16) (field $y i16)))
    (func (export "point_point") (param $x i32) (param $y i32) (result (ref $point))
        (struct.new $point (local.get $x) (local.get $y))
    )
    (func (export "point_x_u") (param $this (ref $point)) (result i32)
        (struct.get_u $point 0 (local.get $this))
    )
    (func (export "point_x_s") (param $this (ref $point)) (result i32)
        (struct.get_s $point 0 (local.get $this))
    )
)
 */

function test() {
    const instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x94\x80\x80\x80\x00\x03\x5f\x02\x77\x00\x77\x00\x60\x02\x7f\x7f\x01\x64\x00\x60\x01\x64\x00\x01\x7f\x03\x84\x80\x80\x80\x00\x03\x01\x02\x02\x07\xa7\x80\x80\x80\x00\x03\x0b\x70\x6f\x69\x6e\x74\x5f\x70\x6f\x69\x6e\x74\x00\x00\x09\x70\x6f\x69\x6e\x74\x5f\x78\x5f\x75\x00\x01\x09\x70\x6f\x69\x6e\x74\x5f\x78\x5f\x73\x00\x02\x0a\xa9\x80\x80\x80\x00\x03\x89\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfb\x00\x00\x0b\x88\x80\x80\x80\x00\x00\x20\x00\xfb\x04\x00\x00\x0b\x88\x80\x80\x80\x00\x00\x20\x00\xfb\x03\x00\x00\x0b"));
    const { point_point, point_x_u, point_x_s } = instance.exports
    let point = point_point(-5, 3);
    assert.eq(point_x_s(point), -5);
    assert.eq(point_x_u(point), 65531);
}

test()

