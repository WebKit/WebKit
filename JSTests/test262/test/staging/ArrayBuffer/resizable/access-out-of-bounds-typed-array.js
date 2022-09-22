// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from AccessOutOfBoundsTypedArray test
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
  // Initial values
  for (let i = 0; i < 4; ++i) {
    assert.sameValue(array[i], 0);
  }
  // Within-bounds write
  for (let i = 0; i < 4; ++i) {
    array[i] = i;
  }
  // Within-bounds read
  for (let i = 0; i < 4; ++i) {
    assert.sameValue(array[i], i);
  }
  rab.resize(2);
  // OOB read. If the RAB isn't large enough to fit the entire TypedArray,
  // the length of the TypedArray is treated as 0.
  for (let i = 0; i < 4; ++i) {
    assert.sameValue(array[i], undefined);
  }
  // OOB write (has no effect)
  for (let i = 0; i < 4; ++i) {
    array[i] = 10;
  }
  rab.resize(4);
  // Within-bounds read
  for (let i = 0; i < 2; ++i) {
    assert.sameValue(array[i], i);
  }
  // The shrunk-and-regrown part got zeroed.
  for (let i = 2; i < 4; ++i) {
    assert.sameValue(array[i], 0);
  }
  rab.resize(40);
  // Within-bounds read
  for (let i = 0; i < 2; ++i) {
    assert.sameValue(array[i], i);
  }
  for (let i = 2; i < 4; ++i) {
    assert.sameValue(array[i], 0);
  }
}
