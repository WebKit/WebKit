// Copyright 2023 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.lastindexof
description: >
  Array.p.lastIndexOf behaves correctly on TypedArrays backed by resizable
  buffers.
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
    taWrite[i] = MayNeedBigInt(taWrite, Math.floor(i / 2));
  }

  // Orig. array: [0, 0, 1, 1]
  //              [0, 0, 1, 1] << fixedLength
  //                    [1, 1] << fixedLengthWithOffset
  //              [0, 0, 1, 1, ...] << lengthTracking
  //                    [1, 1, ...] << lengthTrackingWithOffset

  // If fixedLength is a BigInt array, they all are BigInt Arrays.
  let n0 = MayNeedBigInt(fixedLength, 0);
  let n1 = MayNeedBigInt(fixedLength, 1);

  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n0), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n0, 1), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n0, 2), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n0, -2), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n0, -3), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n1, 1), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n1, -2), 2);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n1, -3), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, undefined), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, n0), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, n1), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, n1, -2), 0);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, n1, -1), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, undefined), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, n0), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, n0, 2), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, n0, -3), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, n1, 1), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, n1, 2), 2);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, n1, -3), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, undefined), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n0), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n1), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n1, 1), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n1, -2), 0);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n1, -1), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, undefined), -1);

  // Shrink so that fixed length TAs go out of bounds.
  rab.resize(3 * ctor.BYTES_PER_ELEMENT);

  // Orig. array: [0, 0, 1]
  //              [0, 0, 1, ...] << lengthTracking
  //                    [1, ...] << lengthTrackingWithOffset

  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n1), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, n1), -1);

  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, n0), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, undefined), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n0), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n1), 0);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, undefined), -1);

  // Shrink so that the TAs with offset go out of bounds.
  rab.resize(1 * ctor.BYTES_PER_ELEMENT);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n0), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, n0), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n0), -1);

  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, n0), 0);

  // Shrink to zero.
  rab.resize(0);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n0), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, n0), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n0), -1);

  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, n0), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, undefined), -1);

  // Grow so that all TAs are back in-bounds.
  rab.resize(6 * ctor.BYTES_PER_ELEMENT);
  for (let i = 0; i < 6; ++i) {
    taWrite[i] = MayNeedBigInt(taWrite, Math.floor(i / 2));
  }

  // Orig. array: [0, 0, 1, 1, 2, 2]
  //              [0, 0, 1, 1] << fixedLength
  //                    [1, 1] << fixedLengthWithOffset
  //              [0, 0, 1, 1, 2, 2, ...] << lengthTracking
  //                    [1, 1, 2, 2, ...] << lengthTrackingWithOffset

  let n2 = MayNeedBigInt(fixedLength, 2);

  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n1), 3);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, n2), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLength, undefined), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, n0), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, n1), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, n2), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(fixedLengthWithOffset, undefined), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, n1), 3);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, n2), 5);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTracking, undefined), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n0), -1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n1), 1);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, n2), 3);
  assert.sameValue(Array.prototype.lastIndexOf.call(lengthTrackingWithOffset, undefined), -1);
}
