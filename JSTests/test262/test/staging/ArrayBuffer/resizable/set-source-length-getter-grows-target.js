// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from SetSourceLengthGetterGrowsTarget test
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

// Orig. array: [0, 2, 4, 6]
//              [0, 2, 4, 6] << fixedLength
//                    [4, 6] << fixedLengthWithOffset
//              [0, 2, 4, 6, ...] << lengthTracking
//                    [4, 6, ...] << lengthTrackingWithOffset
function CreateRabForTest(ctor) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  // Write some data into the array.
  const taWrite = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, 2 * i);
  }
  return rab;
}
let rab;
let resizeTo;
function CreateSourceProxy(length) {
  return new Proxy({}, {
    get(target, prop, receiver) {
      if (prop == 'length') {
        rab.resize(resizeTo);
        return length;
      }
      return true; // Can be converted to both BigInt and Number.
    }
  });
}

// Test that we still throw for lengthTracking TAs if the source length is
// too large, even though we resized in the length getter (we check against
// the original length).
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab, 0);
  resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
  assert.throws(RangeError, () => {
    lengthTracking.set(CreateSourceProxy(6));
  });
  assert.compareArray(ToNumbers(new ctor(rab)), [
    0,
    2,
    4,
    6,
    0,
    0
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  resizeTo = 6 * ctor.BYTES_PER_ELEMENT;
  assert.throws(RangeError, () => {
    lengthTrackingWithOffset.set(CreateSourceProxy(4));
  });
  assert.compareArray(ToNumbers(new ctor(rab)), [
    0,
    2,
    4,
    6,
    0,
    0
  ]);
}
