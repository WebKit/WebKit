// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from JoinParameterConversionShrinks test
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

const TypedArrayJoinHelper = (ta, ...rest) => {
  return ta.join(...rest);
};

const ArrayJoinHelper = (ta, ...rest) => {
  return Array.prototype.join.call(ta, ...rest);
};

function JoinParameterConversionShrinks(joinHelper) {
  // Shrinking + fixed-length TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    let evil = {
      toString: () => {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
        return '.';
      }
    };
    // We iterate 4 elements, since it was the starting length, but the TA is
    // OOB right after parameter conversion, so all elements are converted to
    // the empty string.
    assert.sameValue(joinHelper(fixedLength, evil), '...');
  }

  // Shrinking + length-tracking TA.
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    let evil = {
      toString: () => {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
        return '.';
      }
    };
    // We iterate 4 elements, since it was the starting length. Elements beyond
    // the new length are converted to the empty string.
    assert.sameValue(joinHelper(lengthTracking, evil), '0.0..');
  }
}

JoinParameterConversionShrinks(TypedArrayJoinHelper);
JoinParameterConversionShrinks(ArrayJoinHelper);
