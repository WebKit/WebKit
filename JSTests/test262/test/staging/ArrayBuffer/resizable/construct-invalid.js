// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from ConstructInvalid test
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

const rab = CreateResizableArrayBuffer(40, 80);
for (let ctor of ctors) {
  // Length too big.
  assert.throws(RangeError, () => {
    new ctor(rab, 0, 40 / ctor.BYTES_PER_ELEMENT + 1);
  });
  // Offset too close to the end.
  assert.throws(RangeError, () => {
    new ctor(rab, 40 - ctor.BYTES_PER_ELEMENT, 2);
  });
  // Offset beyond end.
  assert.throws(RangeError, () => {
    new ctor(rab, 40, 1);
  });
  if (ctor.BYTES_PER_ELEMENT > 1) {
    // Offset not a multiple of the byte size.
    assert.throws(RangeError, () => {
      new ctor(rab, 1, 1);
    });
    assert.throws(RangeError, () => {
      new ctor(rab, 1);
    });
  }
}
