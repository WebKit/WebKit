// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from ForEachReduceReduceRightShrinkMidIteration test
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
let values;
let rab;
let resizeAfter;
let resizeTo;
function CollectValuesAndResize(n) {
  if (typeof n == 'bigint') {
    values.push(Number(n));
  } else {
    values.push(n);
  }
  if (values.length == resizeAfter) {
    rab.resize(resizeTo);
  }
  return true;
}
function ForEachHelper(array) {
  values = [];
  array.forEach(CollectValuesAndResize);
  return values;
}
function ReduceHelper(array) {
  values = [];
  array.reduce((acc, n) => {
    CollectValuesAndResize(n);
  }, 'initial value');
  return values;
}
function ReduceRightHelper(array) {
  values = [];
  array.reduceRight((acc, n) => {
    CollectValuesAndResize(n);
  }, 'initial value');
  return values;
}

// Test for forEach.

for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  resizeAfter = 2;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  assert.compareArray(ForEachHelper(fixedLength), [
    0,
    2,
    undefined,
    undefined
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  resizeAfter = 1;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  assert.compareArray(ForEachHelper(fixedLengthWithOffset), [
    4,
    undefined
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab, 0);
  resizeAfter = 2;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  assert.compareArray(ForEachHelper(lengthTracking), [
    0,
    2,
    4,
    undefined
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  resizeAfter = 1;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  assert.compareArray(ForEachHelper(lengthTrackingWithOffset), [
    4,
    undefined
  ]);
}

// Tests for reduce.

for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  resizeAfter = 2;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  assert.compareArray(ReduceHelper(fixedLength), [
    0,
    2,
    undefined,
    undefined
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  resizeAfter = 1;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  assert.compareArray(ReduceHelper(fixedLengthWithOffset), [
    4,
    undefined
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab, 0);
  resizeAfter = 2;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  assert.compareArray(ReduceHelper(lengthTracking), [
    0,
    2,
    4,
    undefined
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  resizeAfter = 1;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  assert.compareArray(ReduceHelper(lengthTrackingWithOffset), [
    4,
    undefined
  ]);
}

// Tests for reduceRight.

for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  resizeAfter = 2;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  assert.compareArray(ReduceRightHelper(fixedLength), [
    6,
    4,
    undefined,
    undefined
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  resizeAfter = 1;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  assert.compareArray(ReduceRightHelper(fixedLengthWithOffset), [
    6,
    undefined
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab, 0);
  resizeAfter = 2;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  // Unaffected by the shrinking, since we've already iterated past the point.
  assert.compareArray(ReduceRightHelper(lengthTracking), [
    6,
    4,
    2,
    0
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab, 0);
  resizeAfter = 1;
  resizeTo = 2 * ctor.BYTES_PER_ELEMENT;
  assert.compareArray(ReduceRightHelper(lengthTracking), [
    6,
    undefined,
    2,
    0
  ]);
}
for (let ctor of ctors) {
  rab = CreateRabForTest(ctor);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  resizeAfter = 1;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  // Unaffected by the shrinking, since we've already iterated past the point.
  assert.compareArray(ReduceRightHelper(lengthTrackingWithOffset), [
    6,
    4
  ]);
}
