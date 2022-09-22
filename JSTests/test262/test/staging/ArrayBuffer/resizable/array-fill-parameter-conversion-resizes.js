// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from ArrayFillParameterConversionResizes test
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

function ReadDataFromBuffer(ab, ctor) {
  let result = [];
  const ta = new ctor(ab, 0, ab.byteLength / ctor.BYTES_PER_ELEMENT);
  for (let item of ta) {
    result.push(Number(item));
  }
  return result;
}

function ArrayFillHelper(ta, n, start, end) {
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    Array.prototype.fill.call(ta, BigInt(n), start, end);
  } else {
    Array.prototype.fill.call(ta, n, start, end);
  }
}

for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const fixedLength = new ctor(rab, 0, 4);
  const evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 3;
    }
  };
  ArrayFillHelper(fixedLength, evil, 1, 2);
  assert.compareArray(ReadDataFromBuffer(rab, ctor), [
    0,
    0
  ]);
}
for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const fixedLength = new ctor(rab, 0, 4);
  const evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 1;
    }
  };
  ArrayFillHelper(fixedLength, 3, evil, 2);
  assert.compareArray(ReadDataFromBuffer(rab, ctor), [
    0,
    0
  ]);
}
for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const fixedLength = new ctor(rab, 0, 4);
  const evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 2;
    }
  };
  ArrayFillHelper(fixedLength, 3, 1, evil);
  assert.compareArray(ReadDataFromBuffer(rab, ctor), [
    0,
    0
  ]);
}
