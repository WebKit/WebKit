// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from Subarray test
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
  // Write some data into the array.
  const taWrite = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, 2 * i);
  }

  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset

  const fixedLengthSubFull = fixedLength.subarray(0);
  assert.compareArray(ToNumbers(fixedLengthSubFull), [
    0,
    2,
    4,
    6
  ]);
  const fixedLengthWithOffsetSubFull = fixedLengthWithOffset.subarray(0);
  assert.compareArray(ToNumbers(fixedLengthWithOffsetSubFull), [
    4,
    6
  ]);
  const lengthTrackingSubFull = lengthTracking.subarray(0);
  assert.compareArray(ToNumbers(lengthTrackingSubFull), [
    0,
    2,
    4,
    6
  ]);
  const lengthTrackingWithOffsetSubFull = lengthTrackingWithOffset.subarray(0);
  assert.compareArray(ToNumbers(lengthTrackingWithOffsetSubFull), [
    4,
    6
  ]);

  // Relative offsets
  assert.compareArray(ToNumbers(fixedLength.subarray(-2)), [
    4,
    6
  ]);
  assert.compareArray(ToNumbers(fixedLengthWithOffset.subarray(-1)), [6]);
  assert.compareArray(ToNumbers(lengthTracking.subarray(-2)), [
    4,
    6
  ]);
  assert.compareArray(ToNumbers(lengthTrackingWithOffset.subarray(-1)), [6]);

  // Shrink so that fixed length TAs go out of bounds.
  rab.resize(3 * ctor.BYTES_PER_ELEMENT);

  // Orig. array: [0, 2, 4]
  //              [0, 2, 4, ...] << lengthTracking
  //                    [4, ...] << lengthTrackingWithOffset

  // We can create subarrays of OOB arrays (which have length 0), as long as
  // the new arrays are not OOB.
  assert.compareArray(ToNumbers(fixedLength.subarray(0)), []);
  assert.compareArray(ToNumbers(fixedLengthWithOffset.subarray(0)), []);
  assert.compareArray(ToNumbers(lengthTracking.subarray(0)), [
    0,
    2,
    4
  ]);
  assert.compareArray(ToNumbers(lengthTrackingWithOffset.subarray(0)), [4]);

  // Also the previously created subarrays are OOB.
  assert.sameValue(fixedLengthSubFull.length, 0);
  assert.sameValue(fixedLengthWithOffsetSubFull.length, 0);

  // Relative offsets
  assert.compareArray(ToNumbers(lengthTracking.subarray(-2)), [
    2,
    4
  ]);
  assert.compareArray(ToNumbers(lengthTrackingWithOffset.subarray(-1)), [4]);

  // Shrink so that the TAs with offset go out of bounds.
  rab.resize(1 * ctor.BYTES_PER_ELEMENT);
  assert.compareArray(ToNumbers(fixedLength.subarray(0)), []);
  assert.compareArray(ToNumbers(lengthTracking.subarray(0)), [0]);

  // Even the 0-length subarray of fixedLengthWithOffset would be OOB ->
  // this throws.
  assert.throws(RangeError, () => {
    fixedLengthWithOffset.subarray(0);
  });

  // Also the previously created subarrays are OOB.
  assert.sameValue(fixedLengthSubFull.length, 0);
  assert.sameValue(fixedLengthWithOffsetSubFull.length, 0);
  assert.sameValue(lengthTrackingWithOffsetSubFull.length, 0);

  // Shrink to zero.
  rab.resize(0);
  assert.compareArray(ToNumbers(fixedLength.subarray(0)), []);
  assert.compareArray(ToNumbers(lengthTracking.subarray(0)), []);
  assert.throws(RangeError, () => {
    fixedLengthWithOffset.subarray(0);
  });
  assert.throws(RangeError, () => {
    lengthTrackingWithOffset.subarray(0);
  });

  // Also the previously created subarrays are OOB.
  assert.sameValue(fixedLengthSubFull.length, 0);
  assert.sameValue(fixedLengthWithOffsetSubFull.length, 0);
  assert.sameValue(lengthTrackingWithOffsetSubFull.length, 0);

  // Grow so that all TAs are back in-bounds.
  rab.resize(6 * ctor.BYTES_PER_ELEMENT);
  for (let i = 0; i < 6; ++i) {
    WriteToTypedArray(taWrite, i, 2 * i);
  }

  // Orig. array: [0, 2, 4, 6, 8, 10]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
  //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset

  assert.compareArray(ToNumbers(fixedLength.subarray(0)), [
    0,
    2,
    4,
    6
  ]);
  assert.compareArray(ToNumbers(fixedLengthWithOffset.subarray(0)), [
    4,
    6
  ]);
  assert.compareArray(ToNumbers(lengthTracking.subarray(0)), [
    0,
    2,
    4,
    6,
    8,
    10
  ]);
  assert.compareArray(ToNumbers(lengthTrackingWithOffset.subarray(0)), [
    4,
    6,
    8,
    10
  ]);

  // Also the previously created subarrays are no longer OOB.
  assert.sameValue(fixedLengthSubFull.length, 4);
  assert.sameValue(fixedLengthWithOffsetSubFull.length, 2);
  // Subarrays of length-tracking TAs are also length-tracking.
  assert.sameValue(lengthTrackingSubFull.length, 6);
  assert.sameValue(lengthTrackingWithOffsetSubFull.length, 4);
}
