// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from TypedArrayLengthAndByteLength test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
includes: [compareArray.js]
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

const rab = CreateResizableArrayBuffer(40, 80);
for (let ctor of ctors) {
  const ta = new ctor(rab, 0, 3);
  assert.compareArray(ta.buffer, rab);
  assert.sameValue(ta.length, 3);
  assert.sameValue(ta.byteLength, 3 * ctor.BYTES_PER_ELEMENT);
  const empty_ta = new ctor(rab, 0, 0);
  assert.compareArray(empty_ta.buffer, rab);
  assert.sameValue(empty_ta.length, 0);
  assert.sameValue(empty_ta.byteLength, 0);
  const ta_with_offset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 3);
  assert.compareArray(ta_with_offset.buffer, rab);
  assert.sameValue(ta_with_offset.length, 3);
  assert.sameValue(ta_with_offset.byteLength, 3 * ctor.BYTES_PER_ELEMENT);
  const empty_ta_with_offset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 0);
  assert.compareArray(empty_ta_with_offset.buffer, rab);
  assert.sameValue(empty_ta_with_offset.length, 0);
  assert.sameValue(empty_ta_with_offset.byteLength, 0);
  const length_tracking_ta = new ctor(rab);
  assert.compareArray(length_tracking_ta.buffer, rab);
  assert.sameValue(length_tracking_ta.length, 40 / ctor.BYTES_PER_ELEMENT);
  assert.sameValue(length_tracking_ta.byteLength, 40);
  const offset = 8;
  const length_tracking_ta_with_offset = new ctor(rab, offset);
  assert.compareArray(length_tracking_ta_with_offset.buffer, rab);
  assert.sameValue(length_tracking_ta_with_offset.length, (40 - offset) / ctor.BYTES_PER_ELEMENT);
  assert.sameValue(length_tracking_ta_with_offset.byteLength, 40 - offset);
  const empty_length_tracking_ta_with_offset = new ctor(rab, 40);
  assert.compareArray(empty_length_tracking_ta_with_offset.buffer, rab);
  assert.sameValue(empty_length_tracking_ta_with_offset.length, 0);
  assert.sameValue(empty_length_tracking_ta_with_offset.byteLength, 0);
}
