// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from LengthTracking1 test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
features: [resizable-arraybuffer]
flags: [onlyStrict]
---*/

class MyUint8Array extends Uint8Array {
}

class MyFloat32Array extends Float32Array {
}

class MyBigInt64Array extends BigInt64Array {
}

const builtinCtors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Float32Array,
  Float64Array,
  Uint8ClampedArray,
  BigUint64Array,
  BigInt64Array
];

const ctors = [
  ...builtinCtors,
  MyUint8Array,
  MyFloat32Array,
  MyBigInt64Array
];

function CreateResizableArrayBuffer(byteLength, maxByteLength) {
  return new ArrayBuffer(byteLength, { maxByteLength: maxByteLength });
}

const rab = CreateResizableArrayBuffer(16, 40);
let tas = [];
for (let ctor of ctors) {
  tas.push(new ctor(rab));
}
for (let ta of tas) {
  assert.sameValue(ta.length, 16 / ta.BYTES_PER_ELEMENT);
  assert.sameValue(ta.byteLength, 16);
}
rab.resize(40);
for (let ta of tas) {
  assert.sameValue(ta.length, 40 / ta.BYTES_PER_ELEMENT);
  assert.sameValue(ta.byteLength, 40);
}
// Resize to a number which is not a multiple of all byte_lengths.
rab.resize(19);
for (let ta of tas) {
  const expected_length = Math.floor(19 / ta.BYTES_PER_ELEMENT);
  assert.sameValue(ta.length, expected_length);
  assert.sameValue(ta.byteLength, expected_length * ta.BYTES_PER_ELEMENT);
}
rab.resize(1);
for (let ta of tas) {
  if (ta.BYTES_PER_ELEMENT == 1) {
    assert.sameValue(ta.length, 1);
    assert.sameValue(ta.byteLength, 1);
  } else {
    assert.sameValue(ta.length, 0);
    assert.sameValue(ta.byteLength, 0);
  }
}
rab.resize(0);
for (let ta of tas) {
  assert.sameValue(ta.length, 0);
  assert.sameValue(ta.byteLength, 0);
}
rab.resize(8);
for (let ta of tas) {
  assert.sameValue(ta.length, 8 / ta.BYTES_PER_ELEMENT);
  assert.sameValue(ta.byteLength, 8);
}
rab.resize(40);
for (let ta of tas) {
  assert.sameValue(ta.length, 40 / ta.BYTES_PER_ELEMENT);
  assert.sameValue(ta.byteLength, 40);
}
