// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from ObjectDefinePropertyDefineProperties test
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

function ObjectDefinePropertyHelper(ta, index, value) {
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    Object.defineProperty(ta, index, { value: BigInt(value) });
  } else {
    Object.defineProperty(ta, index, { value: value });
  }
}

function ObjectDefinePropertiesHelper(ta, index, value) {
  const values = {};
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    values[index] = { value: BigInt(value) };
  } else {
    values[index] = { value: value };
  }
  Object.defineProperties(ta, values);
}

for (let helper of [
    ObjectDefinePropertyHelper,
    ObjectDefinePropertiesHelper
  ]) {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
    const taFull = new ctor(rab, 0);

    // Orig. array: [0, 0, 0, 0]
    //              [0, 0, 0, 0] << fixedLength
    //                    [0, 0] << fixedLengthWithOffset
    //              [0, 0, 0, 0, ...] << lengthTracking
    //                    [0, 0, ...] << lengthTrackingWithOffset

    helper(fixedLength, 0, 1);
    assert.compareArray(ToNumbers(taFull), [
      1,
      0,
      0,
      0
    ]);
    helper(fixedLengthWithOffset, 0, 2);
    assert.compareArray(ToNumbers(taFull), [
      1,
      0,
      2,
      0
    ]);
    helper(lengthTracking, 1, 3);
    assert.compareArray(ToNumbers(taFull), [
      1,
      3,
      2,
      0
    ]);
    helper(lengthTrackingWithOffset, 1, 4);
    assert.compareArray(ToNumbers(taFull), [
      1,
      3,
      2,
      4
    ]);
    assert.throws(TypeError, () => {
      helper(fixedLength, 4, 8);
    });
    assert.throws(TypeError, () => {
      helper(fixedLengthWithOffset, 2, 8);
    });
    assert.throws(TypeError, () => {
      helper(lengthTracking, 4, 8);
    });
    assert.throws(TypeError, () => {
      helper(lengthTrackingWithOffset, 2, 8);
    });

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [1, 3, 2]
    //              [1, 3, 2, ...] << lengthTracking
    //                    [2, ...] << lengthTrackingWithOffset

    assert.throws(TypeError, () => {
      helper(fixedLength, 0, 8);
    });
    assert.throws(TypeError, () => {
      helper(fixedLengthWithOffset, 0, 8);
    });
    assert.compareArray(ToNumbers(taFull), [
      1,
      3,
      2
    ]);
    helper(lengthTracking, 0, 5);
    assert.compareArray(ToNumbers(taFull), [
      5,
      3,
      2
    ]);
    helper(lengthTrackingWithOffset, 0, 6);
    assert.compareArray(ToNumbers(taFull), [
      5,
      3,
      6
    ]);

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    assert.throws(TypeError, () => {
      helper(fixedLength, 0, 8);
    });
    assert.throws(TypeError, () => {
      helper(fixedLengthWithOffset, 0, 8);
    });
    assert.throws(TypeError, () => {
      helper(lengthTrackingWithOffset, 0, 8);
    });
    assert.compareArray(ToNumbers(taFull), [5]);
    helper(lengthTracking, 0, 7);
    assert.compareArray(ToNumbers(taFull), [7]);

    // Shrink to zero.
    rab.resize(0);
    assert.throws(TypeError, () => {
      helper(fixedLength, 0, 8);
    });
    assert.throws(TypeError, () => {
      helper(fixedLengthWithOffset, 0, 8);
    });
    assert.throws(TypeError, () => {
      helper(lengthTracking, 0, 8);
    });
    assert.throws(TypeError, () => {
      helper(lengthTrackingWithOffset, 0, 8);
    });
    assert.compareArray(ToNumbers(taFull), []);

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
    helper(fixedLength, 0, 9);
    assert.compareArray(ToNumbers(taFull), [
      9,
      0,
      0,
      0,
      0,
      0
    ]);
    helper(fixedLengthWithOffset, 0, 10);
    assert.compareArray(ToNumbers(taFull), [
      9,
      0,
      10,
      0,
      0,
      0
    ]);
    helper(lengthTracking, 1, 11);
    assert.compareArray(ToNumbers(taFull), [
      9,
      11,
      10,
      0,
      0,
      0
    ]);
    helper(lengthTrackingWithOffset, 2, 12);
    assert.compareArray(ToNumbers(taFull), [
      9,
      11,
      10,
      0,
      12,
      0
    ]);

    // Trying to define properties out of the fixed-length bounds throws.
    assert.throws(TypeError, () => {
      helper(fixedLength, 5, 13);
    });
    assert.throws(TypeError, () => {
      helper(fixedLengthWithOffset, 3, 13);
    });
    assert.compareArray(ToNumbers(taFull), [
      9,
      11,
      10,
      0,
      12,
      0
    ]);
    helper(lengthTracking, 4, 14);
    assert.compareArray(ToNumbers(taFull), [
      9,
      11,
      10,
      0,
      14,
      0
    ]);
    helper(lengthTrackingWithOffset, 3, 15);
    assert.compareArray(ToNumbers(taFull), [
      9,
      11,
      10,
      0,
      14,
      15
    ]);
  }
}
