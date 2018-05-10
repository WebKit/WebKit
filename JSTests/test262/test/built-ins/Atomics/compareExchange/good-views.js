// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.compareexchange
description: Test Atomics.compareExchange on arrays that allow atomic operations.
includes: [testAtomics.js, testTypedArray.js]
features: [ArrayBuffer, arrow-function, Atomics, DataView, for-of, let, SharedArrayBuffer, TypedArray]
---*/

var sab = new SharedArrayBuffer(1024);
var ab = new ArrayBuffer(16);
var views = intArrayConstructors.slice();

testWithTypedArrayConstructors(function(TA) {
  // Make it interesting - use non-zero byteOffsets and non-zero indexes.

  var view = new TA(sab, 32, 20);
  var control = new TA(ab, 0, 2);

  // Performs the exchange
  view[8] = 0;
  assert.sameValue(Atomics.compareExchange(view, 8, 0, 10), 0);
  assert.sameValue(view[8], 10);

  view[8] = 0;
  assert.sameValue(Atomics.compareExchange(view, 8, 1, 10), 0,
    "Does not perform the exchange");
  assert.sameValue(view[8], 0);

  view[8] = 0;
  assert.sameValue(Atomics.compareExchange(view, 8, 0, -5), 0,
    "Performs the exchange, coercing the value being stored");
  control[0] = -5;
  assert.sameValue(view[8], control[0]);


  view[3] = -5;
  control[0] = -5;
  assert.sameValue(Atomics.compareExchange(view, 3, -5, 0), control[0],
    "Performs the exchange, coercing the value being tested");
  assert.sameValue(view[3], 0);


  control[0] = 12345;
  view[3] = 12345;
  assert.sameValue(Atomics.compareExchange(view, 3, 12345, 0), control[0],
    "Performs the exchange, chopping the value being tested");
  assert.sameValue(view[3], 0);

  control[0] = 123456789;
  view[3] = 123456789;
  assert.sameValue(Atomics.compareExchange(view, 3, 123456789, 0), control[0],
    "Performs the exchange, chopping the value being tested");
  assert.sameValue(view[3], 0);

  // In-bounds boundary cases for indexing
  testWithAtomicsInBoundsIndices(function(IdxGen) {
    let Idx = IdxGen(view);
    view.fill(0);
    // Atomics.store() computes an index from Idx in the same way as other
    // Atomics operations, not quite like view[Idx].
    Atomics.store(view, Idx, 37);
    assert.sameValue(Atomics.compareExchange(view, Idx, 37, 0), 37);
  });
}, views);
