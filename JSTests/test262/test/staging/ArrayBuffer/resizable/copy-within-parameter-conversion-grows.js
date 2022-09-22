// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from CopyWithinParameterConversionGrows test
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
  const lengthTracking = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(lengthTracking, i, i);
  }
  const evil = {
    valueOf: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      WriteToTypedArray(lengthTracking, 4, 4);
      WriteToTypedArray(lengthTracking, 5, 5);
      return 0;
    }
  };
  // Orig. array: [0, 1, 2, 3]  [4, 5]
  //               ^     ^       ^ new elements
  //          target     start
  lengthTracking.copyWithin(evil, 2);
  assert.compareArray(ToNumbers(lengthTracking), [
    2,
    3,
    2,
    3,
    4,
    5
  ]);
  rab.resize(4 * ctor.BYTES_PER_ELEMENT);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(lengthTracking, i, i);
  }

  // Orig. array: [0, 1, 2, 3]  [4, 5]
  //               ^     ^       ^ new elements
  //           start     target
  lengthTracking.copyWithin(2, evil);
  assert.compareArray(ToNumbers(lengthTracking), [
    0,
    1,
    0,
    1,
    4,
    5
  ]);
}
