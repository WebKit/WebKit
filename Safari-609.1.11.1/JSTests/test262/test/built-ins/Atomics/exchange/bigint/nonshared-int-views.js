// Copyright (C) 2018 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-atomics.exchange
description: >
  Test Atomics.exchange on non-shared integer TypedArrays
includes: [testBigIntTypedArray.js]
features: [ArrayBuffer, arrow-function, Atomics, BigInt, TypedArray]
---*/
var buffer = new ArrayBuffer(BigInt64Array.BYTES_PER_ELEMENT * 2);

testWithBigIntTypedArrayConstructors(function(TA) {
  assert.throws(TypeError, function() {
    Atomics.exchange(new TA(buffer), 0n, 0n);
  }, '`Atomics.exchange(new TA(buffer), 0n, 0n)` throws TypeError');
});