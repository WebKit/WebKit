// Copyright 2023 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.findlastindex
description: >
  Array.p.findLastIndex behaves correctly when receiver is backed by resizable
  buffer
includes: [resizableArrayBufferUtils.js]
features: [resizable-arraybuffer]
---*/

for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const fixedLength = new ctor(rab, 0, 4);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  const lengthTracking = new ctor(rab, 0);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

  // Write some data into the array.
  const taWrite = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, 2 * i);
  }

  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset

  function isTwoOrFour(n) {
    return n == 2 || n == 4;
  }
  assert.sameValue(Array.prototype.findLastIndex.call(fixedLength, isTwoOrFour), 2);
  assert.sameValue(Array.prototype.findLastIndex.call(fixedLengthWithOffset, isTwoOrFour), 0);
  assert.sameValue(Array.prototype.findLastIndex.call(lengthTracking, isTwoOrFour), 2);
  assert.sameValue(Array.prototype.findLastIndex.call(lengthTrackingWithOffset, isTwoOrFour), 0);

  // Shrink so that fixed length TAs go out of bounds.
  rab.resize(3 * ctor.BYTES_PER_ELEMENT);

  // Orig. array: [0, 2, 4]
  //              [0, 2, 4, ...] << lengthTracking
  //                    [4, ...] << lengthTrackingWithOffset

  assert.sameValue(Array.prototype.findLastIndex.call(fixedLength, isTwoOrFour), -1);
  assert.sameValue(Array.prototype.findLastIndex.call(fixedLengthWithOffset, isTwoOrFour), -1);

  assert.sameValue(Array.prototype.findLastIndex.call(lengthTracking, isTwoOrFour), 2);
  assert.sameValue(Array.prototype.findLastIndex.call(lengthTrackingWithOffset, isTwoOrFour), 0);

  // Shrink so that the TAs with offset go out of bounds.
  rab.resize(1 * ctor.BYTES_PER_ELEMENT);
  assert.sameValue(Array.prototype.findLastIndex.call(fixedLength, isTwoOrFour), -1);
  assert.sameValue(Array.prototype.findLastIndex.call(fixedLengthWithOffset, isTwoOrFour), -1);
  assert.sameValue(Array.prototype.findLastIndex.call(lengthTrackingWithOffset, isTwoOrFour), -1);

  assert.sameValue(Array.prototype.findLastIndex.call(lengthTracking, isTwoOrFour), -1);

  // Shrink to zero.
  rab.resize(0);
  assert.sameValue(Array.prototype.findLastIndex.call(fixedLength, isTwoOrFour), -1);
  assert.sameValue(Array.prototype.findLastIndex.call(fixedLengthWithOffset, isTwoOrFour), -1);
  assert.sameValue(Array.prototype.findLastIndex.call(lengthTrackingWithOffset, isTwoOrFour), -1);

  assert.sameValue(Array.prototype.findLastIndex.call(lengthTracking, isTwoOrFour), -1);

  // Grow so that all TAs are back in-bounds.
  rab.resize(6 * ctor.BYTES_PER_ELEMENT);
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, 0);
  }
  WriteToTypedArray(taWrite, 4, 2);
  WriteToTypedArray(taWrite, 5, 4);

  // Orig. array: [0, 0, 0, 0, 2, 4]
  //              [0, 0, 0, 0] << fixedLength
  //                    [0, 0] << fixedLengthWithOffset
  //              [0, 0, 0, 0, 2, 4, ...] << lengthTracking
  //                    [0, 0, 2, 4, ...] << lengthTrackingWithOffset

  assert.sameValue(Array.prototype.findLastIndex.call(fixedLength, isTwoOrFour), -1);
  assert.sameValue(Array.prototype.findLastIndex.call(fixedLengthWithOffset, isTwoOrFour), -1);
  assert.sameValue(Array.prototype.findLastIndex.call(lengthTracking, isTwoOrFour), 5);
  assert.sameValue(Array.prototype.findLastIndex.call(lengthTrackingWithOffset, isTwoOrFour), 3);
}

