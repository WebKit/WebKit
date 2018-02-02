// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Test range checking of Atomics.wake on arrays that allow atomic operations
includes: [testAtomics.js, testTypedArray.js]
features: [SharedArrayBuffer, ArrayBuffer, DataView, Atomics, TypedArray, arrow-function, let, for-of]
---*/

var sab = new SharedArrayBuffer(8);
var views = [Int32Array];

if (typeof BigInt !== "undefined") {
  views.push(BigInt64Array);
}

testWithTypedArrayConstructors(function(View) {
  let view = new View(sab);
  testWithAtomicsOutOfBoundsIndices(function(IdxGen) {
    let Idx = IdxGen(view);
    assert.throws(RangeError, () => Atomics.wake(view, Idx, 0)); // Even with waking zero
  });
}, views);
