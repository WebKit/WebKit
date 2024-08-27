// Copyright 2023 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%typedarray%.prototype.map
description: >
  TypedArray.p.map behaves correctly on TypedArrays backed by resizable buffers
  that are shrunk mid-iteration.
includes: [compareArray.js, resizableArrayBufferUtils.js]
features: [resizable-arraybuffer]
---*/

let values;
let rab;
let resizeAfter;
let resizeTo;
// Collects the view of the resizable array buffer rab into values, with an
// iteration during which, after resizeAfter steps, rab is resized to length
// resizeTo. To be called by a method of the view being collected.
// Note that rab, values, resizeAfter, and resizeTo may need to be reset
// before calling this. This version can deal with the undefined values
// resulting by shrinking rab.
function ShrinkMidIteration(n, ix, ta) {
  CollectValuesAndResize(n, values, rab, resizeAfter, resizeTo);
  // We still need to return a valid BigInt / non-BigInt, even if
  // n is `undefined`.
  if (ta instanceof BigInt64Array || ta instanceof BigUint64Array) {
    return 0n;
  }
  return 0;
}

// Orig. array: [0, 2, 4, 6]
//              [0, 2, 4, 6] << fixedLength
//                    [4, 6] << fixedLengthWithOffset
//              [0, 2, 4, 6, ...] << lengthTracking
//                    [4, 6, ...] << lengthTrackingWithOffset
for (let ctor of ctors) {
  values = [];
  rab = CreateRabForTest(ctor);
  const fixedLength = new ctor(rab, 0, 4);
  resizeAfter = 2;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  fixedLength.map(ShrinkMidIteration);
  assert.compareArray(values, [
    0,
    2,
    undefined,
    undefined
  ]);
}
for (let ctor of ctors) {
  values = [];
  rab = CreateRabForTest(ctor);
  const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
  resizeAfter = 1;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  fixedLengthWithOffset.map(ShrinkMidIteration);
  assert.compareArray(values, [
    4,
    undefined
  ]);
}
for (let ctor of ctors) {
  values = [];
  rab = CreateRabForTest(ctor);
  const lengthTracking = new ctor(rab, 0);
  resizeAfter = 2;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  lengthTracking.map(ShrinkMidIteration);
  assert.compareArray(values, [
    0,
    2,
    4,
    undefined
  ]);
}
for (let ctor of ctors) {
  values = [];
  rab = CreateRabForTest(ctor);
  const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);
  resizeAfter = 1;
  resizeTo = 3 * ctor.BYTES_PER_ELEMENT;
  lengthTrackingWithOffset.map(ShrinkMidIteration);
  assert.compareArray(values, [
    4,
    undefined
  ]);
}
