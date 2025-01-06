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
    (type $ai (array (mut i32)))
    (func (export "array_array") (param $n i32) (param $x i32) (result (ref $ai))
        (array.new $ai (local.get $x) (local.get $n))
    )
    (func (export "array_array_default") (param $n i32) (result (ref $ai))
        (array.new_default $ai (local.get $n))
    )
    (func (export "array_array_fixed2") (param $x i32) (param $y i32) (result (ref $ai))
        (array.new_fixed $ai 2 (local.get $x) (local.get $y))
    )
    (func (export "array_get") (param $arr (ref $ai)) (param $i i32) (result i32)
        (array.get $ai (local.get $arr) (local.get $i))
    )
)
 */

function test() {

    const instance = new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x98\x80\x80\x80\x00\x04\x5e\x7f\x01\x60\x02\x7f\x7f\x01\x64\x00\x60\x01\x7f\x01\x64\x00\x60\x02\x64\x00\x7f\x01\x7f\x03\x85\x80\x80\x80\x00\x04\x01\x02\x01\x03\x07\xc6\x80\x80\x80\x00\x04\x0b\x61\x72\x72\x61\x79\x5f\x61\x72\x72\x61\x79\x00\x00\x13\x61\x72\x72\x61\x79\x5f\x61\x72\x72\x61\x79\x5f\x64\x65\x66\x61\x75\x6c\x74\x00\x01\x12\x61\x72\x72\x61\x79\x5f\x61\x72\x72\x61\x79\x5f\x66\x69\x78\x65\x64\x32\x00\x02\x09\x61\x72\x72\x61\x79\x5f\x67\x65\x74\x00\x03\x0a\xb8\x80\x80\x80\x00\x04\x89\x80\x80\x80\x00\x00\x20\x01\x20\x00\xfb\x06\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\xfb\x07\x00\x0b\x8a\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfb\x08\x00\x02\x0b\x89\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfb\x0b\x00\x0b"));
    const { array_array, array_array_default, array_array_fixed2, array_get } = instance.exports
    let arr = array_array(5, 2);
    assert.eq(array_get(arr, 0), 2);
    assert.eq(array_get(arr, 4), 2);

    try {
        array_get(arr, 5);
        assert.eq(0, 1);
    } catch (error) {
    }
    
    let arr2 = array_array_default(6);
    assert.eq(array_get(arr2, 0), 0);
    assert.eq(array_get(arr2, 5), 0);

    let arr3 = array_array_fixed2(3, 5);
    assert.eq(array_get(arr3, 0), 3);
    assert.eq(array_get(arr3, 1), 5);
}

test()

