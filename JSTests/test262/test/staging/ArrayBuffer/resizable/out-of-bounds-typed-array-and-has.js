// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from OutOfBoundsTypedArrayAndHas test
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

for (let ctor of ctors) {
  if (ctor.BYTES_PER_ELEMENT != 1) {
    continue;
  }
  const rab = CreateResizableArrayBuffer(16, 40);
  const array = new ctor(rab, 0, 4);
  // Within-bounds read
  for (let i = 0; i < 4; ++i) {
    assert(i in array);
  }
  rab.resize(2);
  // OOB read. If the RAB isn't large enough to fit the entire TypedArray,
  // the length of the TypedArray is treated as 0.
  for (let i = 0; i < 4; ++i) {
    assert(!(i in array));
  }
  rab.resize(4);
  // Within-bounds read
  for (let i = 0; i < 4; ++i) {
    assert(i in array);
  }
  rab.resize(40);
  // Within-bounds read
  for (let i = 0; i < 4; ++i) {
    assert(i in array);
  }
}
