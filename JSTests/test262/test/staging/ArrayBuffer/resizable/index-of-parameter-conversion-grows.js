// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from IndexOfParameterConversionGrows test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
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

function TypedArrayIndexOfHelper(ta, n, fromIndex) {
  if (typeof n == 'number' && (ta instanceof BigInt64Array || ta instanceof BigUint64Array)) {
    if (fromIndex == undefined) {
      return ta.indexOf(BigInt(n));
    }
    return ta.indexOf(BigInt(n), fromIndex);
  }
  if (fromIndex == undefined) {
    return ta.indexOf(n);
  }
  return ta.indexOf(n, fromIndex);
}

function ArrayIndexOfHelper(ta, n, fromIndex) {
  if (typeof n == 'number' && (ta instanceof BigInt64Array || ta instanceof BigUint64Array)) {
    if (fromIndex == undefined) {
      return Array.prototype.indexOf.call(ta, BigInt(n));
    }
    return Array.prototype.indexOf.call(ta, BigInt(n), fromIndex);
  }
  if (fromIndex == undefined) {
    return Array.prototype.indexOf.call(ta, n);
  }
  return Array.prototype.indexOf.call(ta, n, fromIndex);
}

function IndexOfParameterConversionGrows(indexOfHelper) {
  // Growing + length-tracking TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(lengthTracking, i, 1);
    }
    let evil = {
      valueOf: () => {
        rab.resize(6 * ctor.BYTES_PER_ELEMENT);
        return 0;
      }
    };
    assert.sameValue(indexOfHelper(lengthTracking, 0), -1);
    // The TA grew but we only look at the data until the original length.
    assert.sameValue(indexOfHelper(lengthTracking, 0, evil), -1);
  }

  // Growing + length-tracking TA, index conversion.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    WriteToTypedArray(lengthTracking, 0, 1);
    let evil = {
      valueOf: () => {
        rab.resize(6 * ctor.BYTES_PER_ELEMENT);
        return -4;
      }
    };
    assert.sameValue(indexOfHelper(lengthTracking, 1, -4), 0);
    // The TA grew but the start index conversion is done based on the original
    // length.
    assert.sameValue(indexOfHelper(lengthTracking, 1, evil), 0);
  }
}

IndexOfParameterConversionGrows(TypedArrayIndexOfHelper);
IndexOfParameterConversionGrows(ArrayIndexOfHelper);
