// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from LastIndexOfParameterConversionGrows test
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

function TypedArrayLastIndexOfHelper(ta, n, fromIndex) {
  if (typeof n == 'number' && (ta instanceof BigInt64Array || ta instanceof BigUint64Array)) {
    if (fromIndex == undefined) {
      return ta.lastIndexOf(BigInt(n));
    }
    return ta.lastIndexOf(BigInt(n), fromIndex);
  }
  if (fromIndex == undefined) {
    return ta.lastIndexOf(n);
  }
  return ta.lastIndexOf(n, fromIndex);
}

function ArrayLastIndexOfHelper(ta, n, fromIndex) {
  if (typeof n == 'number' && (ta instanceof BigInt64Array || ta instanceof BigUint64Array)) {
    if (fromIndex == undefined) {
      return Array.prototype.lastIndexOf.call(ta, BigInt(n));
    }
    return Array.prototype.lastIndexOf.call(ta, BigInt(n), fromIndex);
  }
  if (fromIndex == undefined) {
    return Array.prototype.lastIndexOf.call(ta, n);
  }
  return Array.prototype.lastIndexOf.call(ta, n, fromIndex);
}

function LastIndexOfParameterConversionGrows(lastIndexOfHelper) {
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
        return -1;
      }
    };
    assert.sameValue(lastIndexOfHelper(lengthTracking, 0), -1);
    // Because lastIndexOf iterates from the given index downwards, it's not
    // possible to test that "we only look at the data until the original
    // length" without also testing that the index conversion happening with the
    // original length.
    assert.sameValue(lastIndexOfHelper(lengthTracking, 0, evil), -1);
  }

  // Growing + length-tracking TA, index conversion.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    let evil = {
      valueOf: () => {
        rab.resize(6 * ctor.BYTES_PER_ELEMENT);
        return -4;
      }
    };
    assert.sameValue(lastIndexOfHelper(lengthTracking, 0, -4), 0);
    // The TA grew but the start index conversion is done based on the original
    // length.
    assert.sameValue(lastIndexOfHelper(lengthTracking, 0, evil), 0);
  }
}

LastIndexOfParameterConversionGrows(TypedArrayLastIndexOfHelper);
LastIndexOfParameterConversionGrows(ArrayLastIndexOfHelper);
