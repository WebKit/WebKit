// Copyright (C) 2018 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.load
description: >
  Test Atomics.load on non-shared integer TypedArrays
includes: [testBigIntTypedArray.js]
features: [ArrayBuffer, arrow-function, Atomics, BigInt, TypedArray]
---*/

var ab = new ArrayBuffer(BigInt64Array.BYTES_PER_ELEMENT * 2);


testWithBigIntTypedArrayConstructors(function(TA) {
  var view = new TA(ab);

  assert.throws(TypeError, function() {
    Atomics.load(view, 0);
  }, '`Atomics.load(view, 0)` throws TypeError');
});
