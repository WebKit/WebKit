// Copyright (C) 2018 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.xor
description: >
  Test range checking of Atomics.xor on arrays that allow atomic operations
includes: [testAtomics.js, testBigIntTypedArray.js]
features: [ArrayBuffer, Atomics, BigInt, DataView, SharedArrayBuffer, Symbol, TypedArray]
---*/

var buffer = new SharedArrayBuffer(BigInt64Array.BYTES_PER_ELEMENT * 2);

testWithBigIntTypedArrayConstructors(function(TA) {
  let view = new TA(buffer);
  testWithAtomicsOutOfBoundsIndices(function(IdxGen) {
    assert.throws(RangeError, function() {
      Atomics.xor(view, IdxGen(view), 10);
    }, '`Atomics.xor(view, IdxGen(view), 10)` throws RangeError');
  });
});
