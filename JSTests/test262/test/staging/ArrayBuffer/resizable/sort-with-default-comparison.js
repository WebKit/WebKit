// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from SortWithDefaultComparison test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
includes: [compareArray.js]
features: [resizable-arraybuffer]
flags: [onlyStrict]
---*/

// This test cannot be reused between TypedArray.protoype.sort and
// Array.prototype.sort, since the default sorting functions differ.

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
  fixedLength.sort();
  assert.compareArray(ToNumbers(taFull), [
    4,
    6,
    8,
    10
  ]);
  WriteUnsortedData();
  fixedLengthWithOffset.sort();
  assert.compareArray(ToNumbers(taFull), [
    10,
    8,
    4,
    6
  ]);
  WriteUnsortedData();
  lengthTracking.sort();
  assert.compareArray(ToNumbers(taFull), [
    4,
    6,
    8,
    10
  ]);
  WriteUnsortedData();
  lengthTrackingWithOffset.sort();
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
  assert.throws(TypeError, () => {
    fixedLength.sort();
  });
  WriteUnsortedData();
  assert.throws(TypeError, () => {
    fixedLengthWithOffset.sort();
  });
  WriteUnsortedData();
  lengthTracking.sort();
  assert.compareArray(ToNumbers(taFull), [
    6,
    8,
    10
  ]);
  WriteUnsortedData();
  lengthTrackingWithOffset.sort();
  assert.compareArray(ToNumbers(taFull), [
    10,
    8,
    6
  ]);

  // Shrink so that the TAs with offset go out of bounds.
  rab.resize(1 * ctor.BYTES_PER_ELEMENT);
  WriteUnsortedData();
  assert.throws(TypeError, () => {
    fixedLength.sort();
  });
  WriteUnsortedData();
  assert.throws(TypeError, () => {
    fixedLengthWithOffset.sort();
  });
  WriteUnsortedData();
  lengthTracking.sort();
  assert.compareArray(ToNumbers(taFull), [10]);
  WriteUnsortedData();
  assert.throws(TypeError, () => {
    lengthTrackingWithOffset.sort();
  });

  // Shrink to zero.
  rab.resize(0);
  WriteUnsortedData();
  assert.throws(TypeError, () => {
    fixedLength.sort();
  });
  WriteUnsortedData();
  assert.throws(TypeError, () => {
    fixedLengthWithOffset.sort();
  });
  WriteUnsortedData();
  lengthTracking.sort();
  assert.compareArray(ToNumbers(taFull), []);
  WriteUnsortedData();
  assert.throws(TypeError, () => {
    lengthTrackingWithOffset.sort();
  });

  // Grow so that all TAs are back in-bounds.
  rab.resize(6 * ctor.BYTES_PER_ELEMENT);

  // Orig. array: [10, 8, 6, 4, 2, 0]
  //              [10, 8, 6, 4] << fixedLength
  //                     [6, 4] << fixedLengthWithOffset
  //              [10, 8, 6, 4, 2, 0, ...] << lengthTracking
  //                     [6, 4, 2, 0, ...] << lengthTrackingWithOffset

  WriteUnsortedData();
  fixedLength.sort();
  assert.compareArray(ToNumbers(taFull), [
    4,
    6,
    8,
    10,
    2,
    0
  ]);
  WriteUnsortedData();
  fixedLengthWithOffset.sort();
  assert.compareArray(ToNumbers(taFull), [
    10,
    8,
    4,
    6,
    2,
    0
  ]);
  WriteUnsortedData();
  lengthTracking.sort();
  assert.compareArray(ToNumbers(taFull), [
    0,
    2,
    4,
    6,
    8,
    10
  ]);
  WriteUnsortedData();
  lengthTrackingWithOffset.sort();
  assert.compareArray(ToNumbers(taFull), [
    10,
    8,
    0,
    2,
    4,
    6
  ]);
}
