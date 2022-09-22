// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from LengthTracking2 test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
features: [resizable-arraybuffer]
flags: [onlyStrict]
---*/

// length-tracking-1 but with offsets.

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
const offset = 8;
let tas = [];
for (let ctor of ctors) {
  tas.push(new ctor(rab, offset));
}
for (let ta of tas) {
  assert.sameValue(ta.length, (16 - offset) / ta.BYTES_PER_ELEMENT);
  assert.sameValue(ta.byteLength, 16 - offset);
  assert.sameValue(ta.byteOffset, offset);
}
rab.resize(40);
for (let ta of tas) {
  assert.sameValue(ta.length, (40 - offset) / ta.BYTES_PER_ELEMENT);
  assert.sameValue(ta.byteLength, 40 - offset);
  assert.sameValue(ta.byteOffset, offset);
}
// Resize to a number which is not a multiple of all byte_lengths.
rab.resize(20);
for (let ta of tas) {
  const expected_length = Math.floor((20 - offset) / ta.BYTES_PER_ELEMENT);
  assert.sameValue(ta.length, expected_length);
  assert.sameValue(ta.byteLength, expected_length * ta.BYTES_PER_ELEMENT);
  assert.sameValue(ta.byteOffset, offset);
}
// Resize so that all TypedArrays go out of bounds (because of the offset).
rab.resize(7);
for (let ta of tas) {
  assert.sameValue(ta.length, 0);
  assert.sameValue(ta.byteLength, 0);
  assert.sameValue(ta.byteOffset, 0);
}
rab.resize(0);
for (let ta of tas) {
  assert.sameValue(ta.length, 0);
  assert.sameValue(ta.byteLength, 0);
  assert.sameValue(ta.byteOffset, 0);
}
rab.resize(8);
for (let ta of tas) {
  assert.sameValue(ta.length, 0);
  assert.sameValue(ta.byteLength, 0);
  assert.sameValue(ta.byteOffset, offset);
}
// Resize so that the TypedArrays which have element size > 1 go out of bounds
// (because less than 1 full element would fit).
rab.resize(offset + 1);
for (let ta of tas) {
  if (ta.BYTES_PER_ELEMENT == 1) {
    assert.sameValue(ta.length, 1);
    assert.sameValue(ta.byteLength, 1);
    assert.sameValue(ta.byteOffset, offset);
  } else {
    assert.sameValue(ta.length, 0);
    assert.sameValue(ta.byteLength, 0);
    assert.sameValue(ta.byteOffset, offset);
  }
}
rab.resize(40);
for (let ta of tas) {
  assert.sameValue(ta.length, (40 - offset) / ta.BYTES_PER_ELEMENT);
  assert.sameValue(ta.byteLength, 40 - offset);
  assert.sameValue(ta.byteOffset, offset);
}
