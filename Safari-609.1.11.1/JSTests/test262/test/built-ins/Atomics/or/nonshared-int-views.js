// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.or
description: >
  Test Atomics.or on non-shared integer TypedArrays
includes: [testTypedArray.js]
features: [ArrayBuffer, Atomics, TypedArray]
---*/

const buffer = new ArrayBuffer(16);
const views = intArrayConstructors.slice();

testWithTypedArrayConstructors(function(TA) {
  assert.throws(TypeError, function() {
    Atomics.or(new TA(buffer), 0, 0);
  }, '`Atomics.or(new TA(buffer), 0, 0)` throws TypeError');
}, views);
