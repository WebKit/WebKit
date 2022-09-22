// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from SetSourceLengthGetterShrinksTarget test
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

// Orig. array: [0, 2, 4, 6]
//              [0, 2, 4, 6] << fixedLength
//                    [4, 6] << fixedLengthWithOffset
//              [0, 2, 4, 6, ...] << lengthTracking
//                    [4, 6, ...] << lengthTrackingWithOffset
function CreateRabForTest(ctor) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  // Write some data into the array.
  const taWrite = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, 2 * i);
  }
  return rab;
}
let rab;
let resizeTo;
function CreateSourceProxy(length) {
  return new Proxy({}, {
    get(target, prop, receiver) {
      if (prop == 'length') {
        rab.resize(resizeTo);
        return length;
      }
      return true; // Can be converted to both BigInt and Number.
    }
  });
}

// Tests where the length getter returns a non-zero value -> these are nop if
// the TA went OOB.
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  fixedLength.set(CreateSourceProxy(1));
  assert.compareArray(ToNumbers(new ctor(rab)), [
    0,
    2,
    4
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  fixedLengthWithOffset.set(CreateSourceProxy(1));
  assert.compareArray(ToNumbers(new ctor(rab)), [
    0,
    2,
    4
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab, 0);
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  lengthTracking.set(CreateSourceProxy(1));
  assert.compareArray(ToNumbers(lengthTracking), [
    1,
    2,
    4
  ]);
  assert.compareArray(ToNumbers(new ctor(rab)), [
    1,
    2,
    4
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  lengthTrackingWithOffset.set(CreateSourceProxy(1));
  assert.compareArray(ToNumbers(lengthTrackingWithOffset), [1]);
  assert.compareArray(ToNumbers(new ctor(rab)), [
    0,
    2,
    1
  ]);
}

// Length-tracking TA goes OOB because of the offset.
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  resizeTo = 1 * ctor.BYTES_PER_ELEMENT;
  lengthTrackingWithOffset.set(CreateSourceProxy(1));
  assert.compareArray(ToNumbers(new ctor(rab)), [0]);
}

// Tests where the length getter returns a zero -> these don't throw even if
// the TA went OOB.
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  fixedLength.set(CreateSourceProxy(0));
  assert.compareArray(ToNumbers(new ctor(rab)), [
    0,
    2,
    4
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  fixedLengthWithOffset.set(CreateSourceProxy(0));
  assert.compareArray(ToNumbers(new ctor(rab)), [
    0,
    2,
    4
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab, 0);
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  lengthTracking.set(CreateSourceProxy(0));
  assert.compareArray(ToNumbers(new ctor(rab)), [
    0,
    2,
    4
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  lengthTrackingWithOffset.set(CreateSourceProxy(0));
  assert.compareArray(ToNumbers(new ctor(rab)), [
    0,
    2,
    4
  ]);
}

// Length-tracking TA goes OOB because of the offset.
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  resizeTo = 1 * ctor.BYTES_PER_ELEMENT;
  lengthTrackingWithOffset.set(CreateSourceProxy(0));
  assert.compareArray(ToNumbers(new ctor(rab)), [0]);
}
