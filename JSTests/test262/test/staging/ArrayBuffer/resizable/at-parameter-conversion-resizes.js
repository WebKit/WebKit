// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from AtParameterConversionResizes test
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

function Convert(item) {
  if (typeof item == 'bigint') {
    return Number(item);
  }
  return item;
}

function TypedArrayAtHelper(ta, index) {
  const result = ta.at(index);
  return Convert(result);
}

function ArrayAtHelper(ta, index) {
  const result = Array.prototype.at.call(ta, index);
  return Convert(result);
}

function AtParameterConversionResizes(atHelper) {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    let evil = {
      valueOf: () => {
        rab.resize(2);
        return 0;
      }
    };
    assert.sameValue(atHelper(fixedLength, evil), undefined);
  }
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    let evil = {
      valueOf: () => {
        rab.resize(2);
        return -1;
      }
    };
    // The TypedArray is *not* out of bounds since it's length-tracking.
    assert.sameValue(atHelper(lengthTracking, evil), undefined);
  }
}

AtParameterConversionResizes(TypedArrayAtHelper);
AtParameterConversionResizes(ArrayAtHelper);
