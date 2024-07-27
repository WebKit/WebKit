// Copyright 2023 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.keys
description: >
  Array.p.keys behaves correctly when receiver is backed by a resizable
  buffer and is grown mid-iteration
features: [resizable-arraybuffer]
includes: [compareArray.js, resizableArrayBufferUtils.js]
---*/

// Orig. array: [0, 2, 4, 6]
//              [0, 2, 4, 6] << fixedLength
//                    [4, 6] << fixedLengthWithOffset
//              [0, 2, 4, 6, ...] << lengthTracking
//                    [4, 6, ...] << lengthTrackingWithOffset

for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  // The fixed length array is not affected by resizing.
  TestIterationAndResize(Array.prototype.keys.call(fixedLength), [
    0,
    1,
    2,
    3
  ], rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
}
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  // The fixed length array is not affected by resizing.
  TestIterationAndResize(Array.prototype.keys.call(fixedLengthWithOffset), [
    0,
    1
  ], rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
}
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab, 0);
  TestIterationAndResize(Array.prototype.keys.call(lengthTracking), [
    0,
    1,
    2,
    3,
    4,
    5
  ], rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
}
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  TestIterationAndResize(Array.prototype.keys.call(lengthTrackingWithOffset), [
    0,
    1,
    2,
    3
  ], rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
}
