// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from ArraySortWithDefaultComparison test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
includes: [compareArray.js]
features: [resizable-arraybuffer]
flags: [onlyStrict]
---*/

// The default comparison function for Array.prototype.sort is the string sort.

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

const ArraySortHelper = (ta, ...rest) => {
  Array.prototype.sort.call(ta, ...rest);
};

for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const fixedLength = new ctor(rab, 0, 4);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  const lengthTracking = new ctor(rab, 0);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  const taFull = new ctor(rab, 0);
  function WriteUnsortedData() {
    // Write some data into the array.
    for (let i = 0; i < taFull.length; ++i) {
      WriteToTypedArray(taFull, i, 10 - 2 * i);
    }
  }
  // Orig. array: [10, 8, 6, 4]
  //              [10, 8, 6, 4] << fixedLength
  //                     [6, 4] << fixedLengthWithOffset
  //              [10, 8, 6, 4, ...] << lengthTracking
  //                     [6, 4, ...] << lengthTrackingWithOffset

  WriteUnsortedData();
  ArraySortHelper(fixedLength);
  assert.compareArray(ToNumbers(taFull), [
    10,
    4,
    6,
    8
  ]);
  WriteUnsortedData();
  ArraySortHelper(fixedLengthWithOffset);
  assert.compareArray(ToNumbers(taFull), [
    10,
    8,
    4,
    6
  ]);
  WriteUnsortedData();
  ArraySortHelper(lengthTracking);
  assert.compareArray(ToNumbers(taFull), [
    10,
    4,
    6,
    8
  ]);
  WriteUnsortedData();
  ArraySortHelper(lengthTrackingWithOffset);
  assert.compareArray(ToNumbers(taFull), [
    10,
    8,
    4,
    6
  ]);

  // Shrink so that fixed length TAs go out of bounds.
  rab.resize(3 * ctor.BYTES_PER_ELEMENT);

  // Orig. array: [10, 8, 6]
  //              [10, 8, 6, ...] << lengthTracking
  //                     [6, ...] << lengthTrackingWithOffset

  WriteUnsortedData();
  ArraySortHelper(fixedLength);  // OOB -> NOOP
  assert.compareArray(ToNumbers(taFull), [
    10,
    8,
    6
  ]);
  ArraySortHelper(fixedLengthWithOffset);  // OOB -> NOOP
  assert.compareArray(ToNumbers(taFull), [
    10,
    8,
    6
  ]);
  ArraySortHelper(lengthTracking);
  assert.compareArray(ToNumbers(taFull), [
    10,
    6,
    8
  ]);
  WriteUnsortedData();
  ArraySortHelper(lengthTrackingWithOffset);
  assert.compareArray(ToNumbers(taFull), [
    10,
    8,
    6
  ]);

  // Shrink so that the TAs with offset go out of bounds.
  rab.resize(1 * ctor.BYTES_PER_ELEMENT);
  WriteUnsortedData();
  ArraySortHelper(fixedLength);  // OOB -> NOOP
  assert.compareArray(ToNumbers(taFull), [10]);
  ArraySortHelper(fixedLengthWithOffset);  // OOB -> NOOP
  assert.compareArray(ToNumbers(taFull), [10]);
  ArraySortHelper(lengthTrackingWithOffset);   // OOB -> NOOP
  assert.compareArray(ToNumbers(taFull), [10]);
  ArraySortHelper(lengthTracking);
  assert.compareArray(ToNumbers(taFull), [10]);

  // Shrink to zero.
  rab.resize(0);
  ArraySortHelper(fixedLength);  // OOB -> NOOP
  assert.compareArray(ToNumbers(taFull), []);
  ArraySortHelper(fixedLengthWithOffset);  // OOB -> NOOP
  assert.compareArray(ToNumbers(taFull), []);
  ArraySortHelper(lengthTrackingWithOffset);  // OOB -> NOOP
  assert.compareArray(ToNumbers(taFull), []);
  ArraySortHelper(lengthTracking);
  assert.compareArray(ToNumbers(taFull), []);

  // Grow so that all TAs are back in-bounds.
  rab.resize(6 * ctor.BYTES_PER_ELEMENT);

  // Orig. array: [10, 8, 6, 4, 2, 0]
  //              [10, 8, 6, 4] << fixedLength
  //                     [6, 4] << fixedLengthWithOffset
  //              [10, 8, 6, 4, 2, 0, ...] << lengthTracking
  //                     [6, 4, 2, 0, ...] << lengthTrackingWithOffset

  WriteUnsortedData();
  ArraySortHelper(fixedLength);
  assert.compareArray(ToNumbers(taFull), [
    10,
    4,
    6,
    8,
    2,
    0
  ]);
  WriteUnsortedData();
  ArraySortHelper(fixedLengthWithOffset);
  assert.compareArray(ToNumbers(taFull), [
    10,
    8,
    4,
    6,
    2,
    0
  ]);
  WriteUnsortedData();
  ArraySortHelper(lengthTracking);
  assert.compareArray(ToNumbers(taFull), [
    0,
    10,
    2,
    4,
    6,
    8
  ]);
  WriteUnsortedData();
  ArraySortHelper(lengthTrackingWithOffset);
  assert.compareArray(ToNumbers(taFull), [
    10,
    8,
    0,
    2,
    4,
    6
  ]);
}
