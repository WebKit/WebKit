// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from MapSpeciesCreateGrows test
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

function IsBigIntTypedArray(ta) {
  return ta instanceof BigInt64Array || ta instanceof BigUint64Array;
}

function WriteToTypedArray(array, index, value) {
  if (array instanceof BigInt64Array || array instanceof BigUint64Array) {
    array[index] = BigInt(value);
  } else {
    array[index] = value;
  }
}

let values;
let rab;
function CollectValues(n, ix, ta) {
  if (typeof n == 'bigint') {
    values.push(Number(n));
  } else {
    values.push(n);
  }
  // We still need to return a valid BigInt / non-BigInt, even if
  // n is `undefined`.
  if (IsBigIntTypedArray(ta)) {
    return 0n;
  }
  return 0;
}
function Helper(array) {
  values = [];
  array.map(CollectValues);
  return values;
}
for (let ctor of ctors) {
  rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const taWrite = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, i);
  }
  let resizeWhenConstructorCalled = false;
  class MyArray extends ctor {
    constructor(...params) {
      super(...params);
      if (resizeWhenConstructorCalled) {
        rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      }
    }
  }
  ;
  const fixedLength = new MyArray(rab, 0, 4);
  resizeWhenConstructorCalled = true;
  assert.compareArray(Helper(fixedLength), [
    0,
    1,
    2,
    3
  ]);
  assert.sameValue(rab.byteLength, 6 * ctor.BYTES_PER_ELEMENT);
}
for (let ctor of ctors) {
  rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const taWrite = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, i);
  }
  let resizeWhenConstructorCalled = false;
  class MyArray extends ctor {
    constructor(...params) {
      super(...params);
      if (resizeWhenConstructorCalled) {
        rab.resize(6 * ctor.BYTES_PER_ELEMENT);
      }
    }
  }
  ;
  const lengthTracking = new MyArray(rab);
  resizeWhenConstructorCalled = true;
  assert.compareArray(Helper(lengthTracking), [
    0,
    1,
    2,
    3
  ]);
  assert.sameValue(rab.byteLength, 6 * ctor.BYTES_PER_ELEMENT);
}
