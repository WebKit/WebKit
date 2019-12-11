// Copyright (C) 2018 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.xor
description: >
  Test Atomics.xor on non-shared integer TypedArrays
includes: [testBigIntTypedArray.js]
features: [ArrayBuffer, arrow-function, Atomics, BigInt, TypedArray]
---*/

var buffer = new ArrayBuffer(BigInt64Array.BYTES_PER_ELEMENT * 4);

testWithBigIntTypedArrayConstructors(function(TA) {
  assert.throws(TypeError, function() {
    Atomics.xor(new TA(buffer), 0, 0);
  }, '`Atomics.xor(new TA(buffer), 0, 0)` throws TypeError');
});
