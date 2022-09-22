// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from ObjectFreeze test
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

// Freezing non-OOB non-zero-length TAs throws.
for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const fixedLength = new ctor(rab, 0, 4);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  const lengthTracking = new ctor(rab, 0);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  assert.throws(TypeError, () => {
    Object.freeze(fixedLength);
  });
  assert.throws(TypeError, () => {
    Object.freeze(fixedLengthWithOffset);
  });
  assert.throws(TypeError, () => {
    Object.freeze(lengthTracking);
  });
  assert.throws(TypeError, () => {
    Object.freeze(lengthTrackingWithOffset);
  });
}
// Freezing zero-length TAs doesn't throw.
for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const fixedLength = new ctor(rab, 0, 0);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 0);
  const lengthTrackingWithOffset = new ctor(rab, 4 * ctor.BYTES_PER_ELEMENT);
  Object.freeze(fixedLength);
  Object.freeze(fixedLengthWithOffset);
  Object.freeze(lengthTrackingWithOffset);
}
// If the buffer has been resized to make length-tracking TAs zero-length,
// freezing them also doesn't throw.
for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const lengthTracking = new ctor(rab);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  rab.resize(2 * ctor.BYTES_PER_ELEMENT);
  Object.freeze(lengthTrackingWithOffset);
  rab.resize(0 * ctor.BYTES_PER_ELEMENT);
  Object.freeze(lengthTracking);
}
