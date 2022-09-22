// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from SubarrayParameterConversionGrows test
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
//              [0, 2, 4, 6, ...] << lengthTracking
function CreateRabForTest(ctor) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  // Write some data into the array.
  const taWrite = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, 2 * i);
  }
  return rab;
}

// Growing a fixed length TA back in bounds.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  // Make `fixedLength` OOB.
  rab.resize(2 * ctor.BYTES_PER_ELEMENT);
  const evil = {
    valueOf: () => {
      rab.resize(4 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }
  };
  // The length computation is done before parameter conversion. At that
  // point, the length is 0, since the TA is OOB.
  assert.compareArray(ToNumbers(fixedLength.subarray(evil, 0, 1)), []);
}

// Growing + fixed-length TA. Growing won't affect anything.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  const evil = {
    valueOf: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }
  };
  assert.compareArray(ToNumbers(fixedLength.subarray(evil)), [
    0,
    2,
    4,
    6
  ]);
}

// Growing + length-tracking TA. The length computation is done with the
// original length.
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab, 0);
  const evil = {
    valueOf: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }
  };
  assert.compareArray(
    ToNumbers(lengthTracking.subarray(evil, lengthTracking.length)), [
    0,
    2,
    4,
    6
  ]);
}
