// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from SliceSpeciesCreateResizes test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
includes: [compareArray.js]
features: [resizable-arraybuffer]
flags: [onlyStrict]
---*/

// The corresponding test for Array.prototype.slice is not possible, since it
// doesn't call the species constructor if the "original array" is not an Array.

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

for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  let resizeWhenConstructorCalled = false;
  class MyArray extends ctor {
    constructor(...params) {
      super(...params);
      if (resizeWhenConstructorCalled) {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      }
    }
  }
  ;
  const fixedLength = new MyArray(rab, 0, 4);
  resizeWhenConstructorCalled = true;
  assert.throws(TypeError, () => {
    fixedLength.slice();
  });
  assert.sameValue(rab.byteLength, 2 * ctor.BYTES_PER_ELEMENT);
}
for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const taWrite = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, 1);
  }
  let resizeWhenConstructorCalled = false;
  class MyArray extends ctor {
    constructor(...params) {
      super(...params);
      if (resizeWhenConstructorCalled) {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      }
    }
  }
  ;
  const lengthTracking = new MyArray(rab);
  resizeWhenConstructorCalled = true;
  const a = lengthTracking.slice();
  assert.sameValue(rab.byteLength, 2 * ctor.BYTES_PER_ELEMENT);
  // The length of the resulting TypedArray is determined before
  // TypedArraySpeciesCreate is called, and it doesn't change.
  assert.sameValue(a.length, 4);
  assert.compareArray(ToNumbers(a), [
    1,
    1,
    0,
    0
  ]);
}

// Test that the (start, end) parameters are computed based on the original
// length.
for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const taWrite = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, 1);
  }
  let resizeWhenConstructorCalled = false;
  class MyArray extends ctor {
    constructor(...params) {
      super(...params);
      if (resizeWhenConstructorCalled) {
        rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      }
    }
  }
  ;
  const lengthTracking = new MyArray(rab);
  resizeWhenConstructorCalled = true;
  const a = lengthTracking.slice(-3, -1);
  assert.sameValue(rab.byteLength, 2 * ctor.BYTES_PER_ELEMENT);
  // The length of the resulting TypedArray is determined before
  // TypedArraySpeciesCreate is called, and it doesn't change.
  assert.sameValue(a.length, 2);
  assert.compareArray(ToNumbers(a), [
    1,
    0
  ]);
}

// Test where the buffer gets resized "between elements".
{
  const rab = CreateResizableArrayBuffer(8, 16);
  const taWrite = new Uint8Array(rab);
  for (let i = 0; i < 8; ++i) {
    WriteToTypedArray(taWrite, i, 255);
  }
  let resizeWhenConstructorCalled = false;
  class MyArray extends Uint16Array {
    constructor(...params) {
      super(...params);
      if (resizeWhenConstructorCalled) {
        rab.resize(5);
      }
    }
  }
  ;
  const lengthTracking = new MyArray(rab);
  assert.compareArray(ToNumbers(lengthTracking), [
    65535,
    65535,
    65535,
    65535
  ]);
  resizeWhenConstructorCalled = true;
  const a = lengthTracking.slice();
  assert.sameValue(rab.byteLength, 5);
  assert.sameValue(a.length, 4);
  assert.sameValue(a[0], 65535);
  assert.sameValue(a[1], 65535);
  assert.sameValue(a[2], 0);
  assert.sameValue(a[3], 0);
}
