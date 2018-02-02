// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Test range checking of Atomics.wait on arrays that allow atomic operations
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
    assert.throws(RangeError, () => Atomics.wait(view, Idx, 10, 0)); // Even with zero timeout
  });
}, views);
