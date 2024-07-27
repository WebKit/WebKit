// Copyright 2023 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.join
description: >
  Array.p.join behaves correctly when the receiver is backed by resizable
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
  const taWrite = new ctor(rab);

  // Write some data into the array.
  for (let i = 0; i < 4; ++i) {
    WriteToTypedArray(taWrite, i, 2 * i);
  }

  // Orig. array: [0, 2, 4, 6]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, ...] << lengthTracking
  //                    [4, 6, ...] << lengthTrackingWithOffset

  assert.sameValue(Array.prototype.join.call(fixedLength), '0,2,4,6');
  assert.sameValue(Array.prototype.join.call(fixedLengthWithOffset), '4,6');
  assert.sameValue(Array.prototype.join.call(lengthTracking), '0,2,4,6');
  assert.sameValue(Array.prototype.join.call(lengthTrackingWithOffset), '4,6');

  // Shrink so that fixed length TAs go out of bounds.
  rab.resize(3 * ctor.BYTES_PER_ELEMENT);

  // Orig. array: [0, 2, 4]
  //              [0, 2, 4, ...] << lengthTracking
  //                    [4, ...] << lengthTrackingWithOffset

  assert.sameValue(Array.prototype.join.call(fixedLength), '');
  assert.sameValue(Array.prototype.join.call(fixedLengthWithOffset), '');

  assert.sameValue(Array.prototype.join.call(lengthTracking), '0,2,4');
  assert.sameValue(Array.prototype.join.call(lengthTrackingWithOffset), '4');

  // Shrink so that the TAs with offset go out of bounds.
  rab.resize(1 * ctor.BYTES_PER_ELEMENT);
  assert.sameValue(Array.prototype.join.call(fixedLength), '');
  assert.sameValue(Array.prototype.join.call(fixedLengthWithOffset), '');
  assert.sameValue(Array.prototype.join.call(lengthTrackingWithOffset), '');

  assert.sameValue(Array.prototype.join.call(lengthTracking), '0');

  // Shrink to zero.
  rab.resize(0);
  assert.sameValue(Array.prototype.join.call(fixedLength), '');
  assert.sameValue(Array.prototype.join.call(fixedLengthWithOffset), '');
  assert.sameValue(Array.prototype.join.call(lengthTrackingWithOffset), '');

  assert.sameValue(Array.prototype.join.call(lengthTracking), '');

  // Grow so that all TAs are back in-bounds.
  rab.resize(6 * ctor.BYTES_PER_ELEMENT);
  for (let i = 0; i < 6; ++i) {
    WriteToTypedArray(taWrite, i, 2 * i);
  }

  // Orig. array: [0, 2, 4, 6, 8, 10]
  //              [0, 2, 4, 6] << fixedLength
  //                    [4, 6] << fixedLengthWithOffset
  //              [0, 2, 4, 6, 8, 10, ...] << lengthTracking
  //                    [4, 6, 8, 10, ...] << lengthTrackingWithOffset

  assert.sameValue(Array.prototype.join.call(fixedLength), '0,2,4,6');
  assert.sameValue(Array.prototype.join.call(fixedLengthWithOffset), '4,6');
  assert.sameValue(Array.prototype.join.call(lengthTracking), '0,2,4,6,8,10');
  assert.sameValue(Array.prototype.join.call(lengthTrackingWithOffset), '4,6,8,10');
}
