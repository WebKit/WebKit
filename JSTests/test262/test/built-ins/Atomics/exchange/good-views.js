// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.exchange
description: Test Atomics.exchange on arrays that allow atomic operations.
includes: [testAtomics.js, testTypedArray.js]
features: [ArrayBuffer, Atomics, DataView, SharedArrayBuffer, Symbol, TypedArray]
---*/

const sab = new SharedArrayBuffer(1024);
const ab = new ArrayBuffer(16);
const views = intArrayConstructors.slice();

testWithTypedArrayConstructors(function(TA) {
  // Make it interesting - use non-zero byteOffsets and non-zero indexes.

  const view = new TA(sab, 32, 20);
  const control = new TA(ab, 0, 2);

  view[8] = 0;
  assert.sameValue(Atomics.exchange(view, 8, 10), 0,
    'Atomics.exchange(view, 8, 10) returns 0');
  assert.sameValue(view[8], 10, 'The value of view[8] is 10');

  assert.sameValue(Atomics.exchange(view, 8, -5), 10,
    'Atomics.exchange(view, 8, -5) returns 10');
  control[0] = -5;
  assert.sameValue(view[8], control[0], 'The value of view[8] equals the value of `control[0]` (-5)');

  view[3] = -5;
  control[0] = -5;
  assert.sameValue(Atomics.exchange(view, 3, 0), control[0],
    'Atomics.exchange(view, 3, 0) returns the value of `control[0]` (-5)');

  control[0] = 12345;
  view[3] = 12345;
  assert.sameValue(Atomics.exchange(view, 3, 0), control[0],
    'Atomics.exchange(view, 3, 0) returns the value of `control[0]` (12345)');

  control[0] = 123456789;
  view[3] = 123456789;
  assert.sameValue(Atomics.exchange(view, 3, 0), control[0],
    'Atomics.exchange(view, 3, 0) returns the value of `control[0]` (123456789)');

  // In-bounds boundary cases for indexing
  testWithAtomicsInBoundsIndices(function(IdxGen) {
    let Idx = IdxGen(view);
    view.fill(0);
    // Atomics.store() computes an index from Idx in the same way as other
    // Atomics operations, not quite like view[Idx].
    Atomics.store(view, Idx, 37);
    assert.sameValue(Atomics.exchange(view, Idx, 0), 37, 'Atomics.exchange(view, Idx, 0) returns 37');
  });
}, views);
