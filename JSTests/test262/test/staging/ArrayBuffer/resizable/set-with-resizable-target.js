// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from SetWithResizableTarget test
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

function SetHelper(target, source, offset) {
  if (target instanceof BigInt64Array || target instanceof BigUint64Array) {
    const bigIntSource = [];
    for (const s of source) {
      bigIntSource.push(BigInt(s));
    }
    source = bigIntSource;
  }
  if (offset == undefined) {
    return target.set(source);
  }
  return target.set(source, offset);
}

for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const fixedLength = new ctor(rab, 0, 4);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  const lengthTracking = new ctor(rab, 0);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  const taFull = new ctor(rab);

  // Orig. array: [0, 0, 0, 0]
  //              [0, 0, 0, 0] << fixedLength
  //                    [0, 0] << fixedLengthWithOffset
  //              [0, 0, 0, 0, ...] << lengthTracking
  //                    [0, 0, ...] << lengthTrackingWithOffset

  // For making sure we're not calling the source length or element getters
  // if the target is OOB.
  const throwingProxy = new Proxy({}, {
    get(target, prop, receiver) {
      throw new Error('Called getter for ' + prop);
    }
  });
  SetHelper(fixedLength, [
    1,
    2
  ]);
  assert.compareArray(ToNumbers(taFull), [
    1,
    2,
    0,
    0
  ]);
  SetHelper(fixedLength, [
    3,
    4
  ], 1);
  assert.compareArray(ToNumbers(taFull), [
    1,
    3,
    4,
    0
  ]);
  assert.throws(RangeError, () => {
    SetHelper(fixedLength, [
      0,
      0,
      0,
      0,
      0
    ]);
  });
  assert.throws(RangeError, () => {
    SetHelper(fixedLength, [
      0,
      0,
      0,
      0
    ], 1);
  });
  assert.compareArray(ToNumbers(taFull), [
    1,
    3,
    4,
    0
  ]);
  SetHelper(fixedLengthWithOffset, [
    5,
    6
  ]);
  assert.compareArray(ToNumbers(taFull), [
    1,
    3,
    5,
    6
  ]);
  SetHelper(fixedLengthWithOffset, [7], 1);
  assert.compareArray(ToNumbers(taFull), [
    1,
    3,
    5,
    7
  ]);
  assert.throws(RangeError, () => {
    SetHelper(fixedLengthWithOffset, [
      0,
      0,
      0
    ]);
  });
  assert.throws(RangeError, () => {
    SetHelper(fixedLengthWithOffset, [
      0,
      0
    ], 1);
  });
  assert.compareArray(ToNumbers(taFull), [
    1,
    3,
    5,
    7
  ]);
  SetHelper(lengthTracking, [
    8,
    9
  ]);
  assert.compareArray(ToNumbers(taFull), [
    8,
    9,
    5,
    7
  ]);
  SetHelper(lengthTracking, [
    10,
    11
  ], 1);
  assert.compareArray(ToNumbers(taFull), [
    8,
    10,
    11,
    7
  ]);
  assert.throws(RangeError, () => {
    SetHelper(lengthTracking, [
      0,
      0,
      0,
      0,
      0
    ]);
  });
  assert.throws(RangeError, () => {
    SetHelper(lengthTracking, [
      0,
      0,
      0,
      0
    ], 1);
  });
  assert.compareArray(ToNumbers(taFull), [
    8,
    10,
    11,
    7
  ]);
  SetHelper(lengthTrackingWithOffset, [
    12,
    13
  ]);
  assert.compareArray(ToNumbers(taFull), [
    8,
    10,
    12,
    13
  ]);
  SetHelper(lengthTrackingWithOffset, [14], 1);
  assert.compareArray(ToNumbers(taFull), [
    8,
    10,
    12,
    14
  ]);
  assert.throws(RangeError, () => {
    SetHelper(lengthTrackingWithOffset, [
      0,
      0,
      0
    ]);
  });
  assert.throws(RangeError, () => {
    SetHelper(lengthTrackingWithOffset, [
      0,
      0
    ], 1);
  });
  assert.compareArray(ToNumbers(taFull), [
    8,
    10,
    12,
    14
  ]);

  // Shrink so that fixed length TAs go out of bounds.
  rab.resize(3 * ctor.BYTES_PER_ELEMENT);

  // Orig. array: [8, 10, 12]
  //              [8, 10, 12, ...] << lengthTracking
  //                     [12, ...] << lengthTrackingWithOffset

  assert.throws(TypeError, () => {
    SetHelper(fixedLength, throwingProxy);
  });
  assert.throws(TypeError, () => {
    SetHelper(fixedLengthWithOffset, throwingProxy);
  });
  assert.compareArray(ToNumbers(taFull), [
    8,
    10,
    12
  ]);
  SetHelper(lengthTracking, [
    15,
    16
  ]);
  assert.compareArray(ToNumbers(taFull), [
    15,
    16,
    12
  ]);
  SetHelper(lengthTracking, [
    17,
    18
  ], 1);
  assert.compareArray(ToNumbers(taFull), [
    15,
    17,
    18
  ]);
  assert.throws(RangeError, () => {
    SetHelper(lengthTracking, [
      0,
      0,
      0,
      0
    ]);
  });
  assert.throws(RangeError, () => {
    SetHelper(lengthTracking, [
      0,
      0,
      0
    ], 1);
  });
  assert.compareArray(ToNumbers(taFull), [
    15,
    17,
    18
  ]);
  SetHelper(lengthTrackingWithOffset, [19]);
  assert.compareArray(ToNumbers(taFull), [
    15,
    17,
    19
  ]);
  assert.throws(RangeError, () => {
    SetHelper(lengthTrackingWithOffset, [
      0,
      0
    ]);
  });
  assert.throws(RangeError, () => {
    SetHelper(lengthTrackingWithOffset, [0], 1);
  });
  assert.compareArray(ToNumbers(taFull), [
    15,
    17,
    19
  ]);

  // Shrink so that the TAs with offset go out of bounds.
  rab.resize(1 * ctor.BYTES_PER_ELEMENT);
  assert.throws(TypeError, () => {
    SetHelper(fixedLength, throwingProxy);
  });
  assert.throws(TypeError, () => {
    SetHelper(fixedLengthWithOffset, throwingProxy);
  });
  assert.throws(TypeError, () => {
    SetHelper(lengthTrackingWithOffset, throwingProxy);
  });
  assert.compareArray(ToNumbers(taFull), [15]);
  SetHelper(lengthTracking, [20]);
  assert.compareArray(ToNumbers(taFull), [20]);

  // Shrink to zero.
  rab.resize(0);
  assert.throws(TypeError, () => {
    SetHelper(fixedLength, throwingProxy);
  });
  assert.throws(TypeError, () => {
    SetHelper(fixedLengthWithOffset, throwingProxy);
  });
  assert.throws(TypeError, () => {
    SetHelper(lengthTrackingWithOffset, throwingProxy);
  });
  assert.throws(RangeError, () => {
    SetHelper(lengthTracking, [0]);
  });
  assert.compareArray(ToNumbers(taFull), []);

  // Grow so that all TAs are back in-bounds.
  rab.resize(6 * ctor.BYTES_PER_ELEMENT);

  // Orig. array: [0, 0, 0, 0, 0, 0]
  //              [0, 0, 0, 0] << fixedLength
  //                    [0, 0] << fixedLengthWithOffset
  //              [0, 0, 0, 0, 0, 0, ...] << lengthTracking
  //                    [0, 0, 0, 0, ...] << lengthTrackingWithOffset
  SetHelper(fixedLength, [
    21,
    22
  ]);
  assert.compareArray(ToNumbers(taFull), [
    21,
    22,
    0,
    0,
    0,
    0
  ]);
  SetHelper(fixedLength, [
    23,
    24
  ], 1);
  assert.compareArray(ToNumbers(taFull), [
    21,
    23,
    24,
    0,
    0,
    0
  ]);
  assert.throws(RangeError, () => {
    SetHelper(fixedLength, [
      0,
      0,
      0,
      0,
      0
    ]);
  });
  assert.throws(RangeError, () => {
    SetHelper(fixedLength, [
      0,
      0,
      0,
      0
    ], 1);
  });
  assert.compareArray(ToNumbers(taFull), [
    21,
    23,
    24,
    0,
    0,
    0
  ]);
  SetHelper(fixedLengthWithOffset, [
    25,
    26
  ]);
  assert.compareArray(ToNumbers(taFull), [
    21,
    23,
    25,
    26,
    0,
    0
  ]);
  SetHelper(fixedLengthWithOffset, [27], 1);
  assert.compareArray(ToNumbers(taFull), [
    21,
    23,
    25,
    27,
    0,
    0
  ]);
  assert.throws(RangeError, () => {
    SetHelper(fixedLengthWithOffset, [
      0,
      0,
      0
    ]);
  });
  assert.throws(RangeError, () => {
    SetHelper(fixedLengthWithOffset, [
      0,
      0
    ], 1);
  });
  assert.compareArray(ToNumbers(taFull), [
    21,
    23,
    25,
    27,
    0,
    0
  ]);
  SetHelper(lengthTracking, [
    28,
    29,
    30,
    31,
    32,
    33
  ]);
  assert.compareArray(ToNumbers(taFull), [
    28,
    29,
    30,
    31,
    32,
    33
  ]);
  SetHelper(lengthTracking, [
    34,
    35,
    36,
    37,
    38
  ], 1);
  assert.compareArray(ToNumbers(taFull), [
    28,
    34,
    35,
    36,
    37,
    38
  ]);
  assert.throws(RangeError, () => {
    SetHelper(lengthTracking, [
      0,
      0,
      0,
      0,
      0,
      0,
      0
    ]);
  });
  assert.throws(RangeError, () => {
    SetHelper(lengthTracking, [
      0,
      0,
      0,
      0,
      0,
      0
    ], 1);
  });
  assert.compareArray(ToNumbers(taFull), [
    28,
    34,
    35,
    36,
    37,
    38
  ]);
  SetHelper(lengthTrackingWithOffset, [
    39,
    40,
    41,
    42
  ]);
  assert.compareArray(ToNumbers(taFull), [
    28,
    34,
    39,
    40,
    41,
    42
  ]);
  SetHelper(lengthTrackingWithOffset, [
    43,
    44,
    45
  ], 1);
  assert.compareArray(ToNumbers(taFull), [
    28,
    34,
    39,
    43,
    44,
    45
  ]);
  assert.throws(RangeError, () => {
    SetHelper(lengthTrackingWithOffset, [
      0,
      0,
      0,
      0,
      0
    ]);
  });
  assert.throws(RangeError, () => {
    SetHelper(lengthTrackingWithOffset, [
      0,
      0,
      0,
      0
    ], 1);
  });
  assert.compareArray(ToNumbers(taFull), [
    28,
    34,
    39,
    43,
    44,
    45
  ]);
}
