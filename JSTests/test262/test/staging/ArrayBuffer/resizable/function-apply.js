// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from FunctionApply test
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

function WriteToTypedArray(array, index, value) {
  if (array instanceof BigInt64Array || array instanceof BigUint64Array) {
    array[index] = BigInt(value);
  } else {
    array[index] = value;
  }
}

function Convert(item) {
  if (typeof item == 'bigint') {
    return Number(item);
  }
  return item;
}

function ToNumbers(array) {
  let result = [];
  for (let item of array) {
    result.push(Convert(item));
  }
  return result;
}

for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const fixedLength = new ctor(rab, 0, 4);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  const lengthTracking = new ctor(rab, 0);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  const taWrite = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, i);
  }
  function func(...args) {
    return [...args];
  }
  assert.compareArray(ToNumbers(func.apply(null, fixedLength)), [
    0,
    1,
    2,
    3
  ]);
  assert.compareArray(ToNumbers(func.apply(null, fixedLengthWithOffset)), [
    2,
    3
  ]);
  assert.compareArray(ToNumbers(func.apply(null, lengthTracking)), [
    0,
    1,
    2,
    3
  ]);
  assert.compareArray(ToNumbers(func.apply(null, lengthTrackingWithOffset)), [
    2,
    3
  ]);

  // Shrink so that fixed length TAs go out of bounds.
  rab.resize(3 * ctor.BYTES_PER_ELEMENT);
  assert.compareArray(ToNumbers(func.apply(null, fixedLength)), []);
  assert.compareArray(ToNumbers(func.apply(null, fixedLengthWithOffset)), []);
  assert.compareArray(ToNumbers(func.apply(null, lengthTracking)), [
    0,
    1,
    2
  ]);
  assert.compareArray(ToNumbers(func.apply(null, lengthTrackingWithOffset)), [2]);

  // Shrink so that the TAs with offset go out of bounds.
  rab.resize(1 * ctor.BYTES_PER_ELEMENT);
  assert.compareArray(ToNumbers(func.apply(null, fixedLength)), []);
  assert.compareArray(ToNumbers(func.apply(null, fixedLengthWithOffset)), []);
  assert.compareArray(ToNumbers(func.apply(null, lengthTracking)), [0]);
  assert.compareArray(ToNumbers(func.apply(null, lengthTrackingWithOffset)), []);

  // Shrink to zero.
  rab.resize(0);
  assert.compareArray(ToNumbers(func.apply(null, fixedLength)), []);
  assert.compareArray(ToNumbers(func.apply(null, fixedLengthWithOffset)), []);
  assert.compareArray(ToNumbers(func.apply(null, lengthTracking)), []);
  assert.compareArray(ToNumbers(func.apply(null, lengthTrackingWithOffset)), []);

  // Grow so that all TAs are back in-bounds. New memory is zeroed.
  rab.resize(6 * ctor.BYTES_PER_ELEMENT);
  assert.compareArray(ToNumbers(func.apply(null, fixedLength)), [
    0,
    0,
    0,
    0
  ]);
  assert.compareArray(ToNumbers(func.apply(null, fixedLengthWithOffset)), [
    0,
    0
  ]);
  assert.compareArray(ToNumbers(func.apply(null, lengthTracking)), [
    0,
    0,
    0,
    0,
    0,
    0
  ]);
  assert.compareArray(ToNumbers(func.apply(null, lengthTrackingWithOffset)), [
    0,
    0,
    0,
    0
  ]);
}
