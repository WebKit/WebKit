// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from ObjectDefinePropertyParameterConversionGrows test
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

const helper = ObjectDefinePropertyHelper;

// Fixed length.
for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const fixedLength = new ctor(rab, 0, 4);
  // Make fixedLength go OOB.
  rab.resize(2 * ctor.BYTES_PER_ELEMENT);
  const evil = {
    toString: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      return 0;
    }
  };
  helper(fixedLength, evil, 8);
  assert.compareArray(ToNumbers(fixedLength), [
    8,
    0,
    0,
    0
  ]);
}

// Length tracking.
for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const lengthTracking = new ctor(rab, 0);
  const evil = {
    toString: () => {
      rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      return 4;  // Index valid after resize.
    }
  };
  helper(lengthTracking, evil, 8);
  assert.compareArray(ToNumbers(lengthTracking), [
    0,
    0,
    0,
    0,
    8,
    0
  ]);
}
