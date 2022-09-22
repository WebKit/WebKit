// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from TypedArrayLengthWhenResizedOutOfBounds1 test
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

// Create TAs which cover the bytes 0-7.
let tas_and_lengths = [];
for (let ctor of ctors) {
  const length = 8 / ctor.BYTES_PER_ELEMENT;
  tas_and_lengths.push([
    new ctor(rab, 0, length),
    length
  ]);
}
for (let [ta, length] of tas_and_lengths) {
  assert.sameValue(ta.length, length);
  assert.sameValue(ta.byteLength, length * ta.BYTES_PER_ELEMENT);
}
rab.resize(2);
for (let [ta, length] of tas_and_lengths) {
  assert.sameValue(ta.length, 0);
  assert.sameValue(ta.byteLength, 0);
}
// Resize the rab so that it just barely covers the needed 8 bytes.
rab.resize(8);
for (let [ta, length] of tas_and_lengths) {
  assert.sameValue(ta.length, length);
  assert.sameValue(ta.byteLength, length * ta.BYTES_PER_ELEMENT);
}
rab.resize(40);
for (let [ta, length] of tas_and_lengths) {
  assert.sameValue(ta.length, length);
  assert.sameValue(ta.byteLength, length * ta.BYTES_PER_ELEMENT);
}
