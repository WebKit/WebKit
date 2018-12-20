// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.and
description: >
  Test Atomics.and on shared non-integer TypedArrays
includes: [testTypedArray.js]
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

const buffer = new SharedArrayBuffer(1024);

testWithTypedArrayConstructors(function(TA) {
  assert.throws(TypeError, function() {
    Atomics.and(new TA(buffer), 0, 0);
  }, '`Atomics.and(new TA(buffer), 0, 0)` throws TypeError');
}, floatArrayConstructors);
