// Copyright 2023 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%typedarray%.prototype.values
description: >
  TypedArray.p.values behaves correctly on TypedArrays backed by resizable
  buffers and resized mid-iteration.
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
  TestIterationAndResize(fixedLength.values(), [
    0,
    2,
    4,
    6
  ], rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
}
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  // The fixed length array is not affected by resizing.
  TestIterationAndResize(fixedLengthWithOffset.values(), [
    4,
    6
  ], rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
}
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab, 0);
  TestIterationAndResize(lengthTracking.values(), [
    0,
    2,
    4,
    6,
    0,
    0
  ], rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
}
for (let ctor of ctors) {
  const rab = CreateRabForTest(ctor);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  TestIterationAndResize(lengthTrackingWithOffset.values(), [
    4,
    6,
    0,
    0
  ], rab, 2, 6 * ctor.BYTES_PER_ELEMENT);
}
