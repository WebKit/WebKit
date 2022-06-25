// Copyright (C) 2018 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-atomics.xor
description: Test Atomics.xor on arrays that allow atomic operations
includes: [testAtomics.js, testBigIntTypedArray.js]
features: [ArrayBuffer, Atomics, BigInt, DataView, SharedArrayBuffer, Symbol, TypedArray]
---*/
// Make it interesting - use non-zero byteOffsets and non-zero indexes.
// And again
// In-bounds boundary cases for indexing
// Atomics.store() computes an index from Idx in the same way as other
// Atomics operations, not quite like view[Idx].
const sab = new SharedArrayBuffer(1024);
const ab = new ArrayBuffer(16);

testWithBigIntTypedArrayConstructors(function(TA) {
  const view = new TA(sab, 32, 20);
  const control = new TA(ab, 0, 2);
  view[8] = 0x33333333n;
  control[0] = 0x33333333n;

  assert.sameValue(
    Atomics.xor(view, 8, 0x55555555n),
    control[0],
    'Atomics.xor(view, 8, 0x55555555n) returns the value of `control[0]` (0x33333333n)'
  );

  control[0] = 0x66666666n;

  assert.sameValue(
    view[8],
    control[0],
    'The value of view[8] equals the value of `control[0]` (0x66666666n)'
  );

  assert.sameValue(
    Atomics.xor(view, 8, 0xF0F0F0F0n),
    control[0],
    'Atomics.xor(view, 8, 0xF0F0F0F0) returns the value of `control[0]` (0x66666666n)'
  );

  control[0] = 0x96969696n;

  assert.sameValue(
    view[8],
    control[0],
    'The value of view[8] equals the value of `control[0]` (0x96969696n)'
  );

  view[3] = -5n;
  control[0] = -5n;

  assert.sameValue(
    Atomics.xor(view, 3, 0n),
    control[0],
    'Atomics.xor(view, 3, 0n) returns the value of `control[0]` (-5n)'
  );

  assert.sameValue(
    view[3],
    control[0],
    'The value of view[3] equals the value of `control[0]` (-5n)'
  );

  control[0] = 12345n;
  view[3] = 12345n;

  assert.sameValue(
    Atomics.xor(view, 3, 0n),
    control[0],
    'Atomics.xor(view, 3, 0n) returns the value of `control[0]` (12345n)'
  );

  assert.sameValue(
    view[3],
    control[0],
    'The value of view[3] equals the value of `control[0]` (12345n)'
  );

  control[0] = 123456789n;
  view[3] = 123456789n;

  assert.sameValue(
    Atomics.xor(view, 3, 0n),
    control[0],
    'Atomics.xor(view, 3, 0n) returns the value of `control[0]` (123456789n)'
  );

  assert.sameValue(
    view[3],
    control[0],
    'The value of view[3] equals the value of `control[0]` (123456789n)'
  );

  testWithAtomicsInBoundsIndices(function(IdxGen) {
    let Idx = IdxGen(view);
    view.fill(0n);
    Atomics.store(view, Idx, 37n);
    assert.sameValue(Atomics.xor(view, Idx, 0n), 37n, 'Atomics.xor(view, Idx, 0n) returns 37n');
  });
});
