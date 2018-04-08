// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.or
description: >
  Test range checking of Atomics.or on arrays that allow atomic operations
includes: [testAtomics.js, testTypedArray.js]
features: [SharedArrayBuffer, ArrayBuffer, DataView, Atomics, arrow-function, let, TypedArray, for-of]
---*/

var sab = new SharedArrayBuffer(8);
var views = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array];

if (typeof BigInt !== "undefined") {
  views.push(BigInt64Array);
  views.push(BigUint64Array);
}

testWithTypedArrayConstructors(function(View) {
  let view = new View(sab);
  testWithAtomicsOutOfBoundsIndices(function(IdxGen) {
    let Idx = IdxGen(view);
    assert.throws(RangeError, () => Atomics.or(view, Idx, 10));
  });
}, views);
