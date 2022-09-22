// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from IncludesParameterConversionResizes test
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

function TypedArrayIncludesHelper(array, n, fromIndex) {
  if (typeof n == 'number' && (array instanceof BigInt64Array || array instanceof BigUint64Array)) {
    return array.includes(BigInt(n), fromIndex);
  }
  return array.includes(n, fromIndex);
}

function ArrayIncludesHelper(array, n, fromIndex) {
  if (typeof n == 'number' && (array instanceof BigInt64Array || array instanceof BigUint64Array)) {
    return Array.prototype.includes.call(array, BigInt(n), fromIndex);
  }
  return Array.prototype.includes.call(array, n, fromIndex);
}

function IncludesParameterConversionResizes(helper) {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    let evil = {
      valueOf: () => {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
        return 0;
      }
    };
    assert(!helper(fixedLength, undefined));
    // The TA is OOB so it includes only "undefined".
    assert(helper(fixedLength, undefined, evil));
  }
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    let evil = {
      valueOf: () => {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
        return 0;
      }
    };
    assert(helper(fixedLength, 0));
    // The TA is OOB so it includes only "undefined".
    assert(!helper(fixedLength, 0, evil));
  }
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(rab);
    let evil = {
      valueOf: () => {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
        return 0;
      }
    };
    assert(!helper(lengthTracking, undefined));
    // "includes" iterates until the original length and sees "undefined"s.
    assert(helper(lengthTracking, undefined, evil));
  }
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
    assert(!helper(lengthTracking, 0));
    // The TA grew but we only look at the data until the original length.
    assert(!helper(lengthTracking, 0, evil));
  }
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
    assert(helper(lengthTracking, 1, -4));
    // The TA grew but the start index conversion is done based on the original
    // length.
    assert(helper(lengthTracking, 1, evil));
  }
}

IncludesParameterConversionResizes(TypedArrayIncludesHelper);
IncludesParameterConversionResizes(ArrayIncludesHelper);
