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
    (func (export "point_x_s") (param $this (ref $point)) (result i32)
        (struct.get_s $point 0 (local.get $this))
    )
    (func (export "point_y_s") (param $this (ref $point)) (result i32)
        (struct.get_s $point 1 (local.get $this))
    )
    (func (export "point_set_x") (param $this (ref $point)) (param $x i32)
        (struct.set $point 0 (local.get $this))
    )
    (func (export "point_set_y") (param $this (ref $point)) (param $y i32)
        (struct.set $point 1 (local.get $this))
    )
)
 */

function test() {
    const instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x9a\x80\x80\x80\x00\x04\x5f\x02\x77\x01\x77\x01\x60\x02\x7f\x7f\x01\x64\x00\x60\x01\x64\x00\x01\x7f\x60\x02\x64\x00\x7f\x00\x03\x86\x80\x80\x80\x00\x05\x01\x02\x02\x03\x03\x07\xc3\x80\x80\x80\x00\x05\x0b\x70\x6f\x69\x6e\x74\x5f\x70\x6f\x69\x6e\x74\x00\x00\x09\x70\x6f\x69\x6e\x74\x5f\x78\x5f\x73\x00\x01\x09\x70\x6f\x69\x6e\x74\x5f\x79\x5f\x73\x00\x02\x0b\x70\x6f\x69\x6e\x74\x5f\x73\x65\x74\x5f\x78\x00\x03\x0b\x70\x6f\x69\x6e\x74\x5f\x73\x65\x74\x5f\x79\x00\x04\x0a\xc7\x80\x80\x80\x00\x05\x89\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfb\x00\x00\x0b\x88\x80\x80\x80\x00\x00\x20\x00\xfb\x03\x00\x00\x0b\x88\x80\x80\x80\x00\x00\x20\x00\xfb\x03\x00\x01\x0b\x8a\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfb\x05\x00\x00\x0b\x8a\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfb\x05\x00\x01\x0b"));
    const { point_point, point_x_s, point_y_s, point_set_x, point_set_y } = instance.exports
    let obj = point_point(1, 2);
    assert.eq(point_x_s(obj), 1);
    assert.eq(point_y_s(obj), 2);

    point_set_x(obj, 5);
    point_set_y(obj, 6);
    assert.eq(point_x_s(obj), 5);
    assert.eq(point_y_s(obj), 6);
}

test()

