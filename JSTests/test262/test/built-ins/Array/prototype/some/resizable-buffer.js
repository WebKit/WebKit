// Copyright 2023 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.some
description: >
  Array.p.some behaves correctly on TypedArrays backed by resizable buffers.
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

  function div3(n) {
    return Number(n) % 3 == 0;
  }
  function over10(n) {
    return Number(n) > 10;
  }
  assert(Array.prototype.some.call(fixedLength, div3));
  assert(!Array.prototype.some.call(fixedLength, over10));
  assert(Array.prototype.some.call(fixedLengthWithOffset, div3));
  assert(!Array.prototype.some.call(fixedLengthWithOffset, over10));
  assert(Array.prototype.some.call(lengthTracking, div3));
  assert(!Array.prototype.some.call(lengthTracking, over10));
  assert(Array.prototype.some.call(lengthTrackingWithOffset, div3));
  assert(!Array.prototype.some.call(lengthTrackingWithOffset, over10));

  // Shrink so that fixed length TAs go out of bounds.
  rab.resize(3 * ctor.BYTES_PER_ELEMENT);

  // Orig. array: [0, 2, 4]
  //              [0, 2, 4, ...] << lengthTracking
  //                    [4, ...] << lengthTrackingWithOffset

  assert(!Array.prototype.some.call(fixedLength, div3));
  assert(!Array.prototype.some.call(fixedLengthWithOffset, div3));

  assert(Array.prototype.some.call(lengthTracking, div3));
  assert(!Array.prototype.some.call(lengthTracking, over10));
  assert(!Array.prototype.some.call(lengthTrackingWithOffset, div3));
  assert(!Array.prototype.some.call(lengthTrackingWithOffset, over10));

  // Shrink so that the TAs with offset go out of bounds.
  rab.resize(1 * ctor.BYTES_PER_ELEMENT);
  assert(!Array.prototype.some.call(fixedLength, div3));
  assert(!Array.prototype.some.call(fixedLengthWithOffset, div3));
  assert(!Array.prototype.some.call(lengthTrackingWithOffset, div3));

  assert(Array.prototype.some.call(lengthTracking, div3));
  assert(!Array.prototype.some.call(lengthTracking, over10));

  // Shrink to zero.
  rab.resize(0);
  assert(!Array.prototype.some.call(fixedLength, div3));
  assert(!Array.prototype.some.call(fixedLengthWithOffset, div3));
  assert(!Array.prototype.some.call(lengthTrackingWithOffset, div3));

  assert(!Array.prototype.some.call(lengthTracking, div3));
  assert(!Array.prototype.some.call(lengthTracking, over10));

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

  assert(Array.prototype.some.call(fixedLength, div3));
  assert(!Array.prototype.some.call(fixedLength, over10));
  assert(Array.prototype.some.call(fixedLengthWithOffset, div3));
  assert(!Array.prototype.some.call(fixedLengthWithOffset, over10));
  assert(Array.prototype.some.call(lengthTracking, div3));
  assert(!Array.prototype.some.call(lengthTracking, over10));
  assert(Array.prototype.some.call(lengthTrackingWithOffset, div3));
  assert(!Array.prototype.some.call(lengthTrackingWithOffset, over10));
}
