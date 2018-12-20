// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.load
description: >
  Test Atomics.load on shared non-integer TypedArrays
includes: [testTypedArray.js]
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

const buffer = new SharedArrayBuffer(1024);
testWithTypedArrayConstructors(function(TA) {
  assert.throws(TypeError, function() {
    Atomics.load(new TA(buffer), 0);
  }, '`Atomics.load(new TA(buffer), 0)` throws TypeError');
}, floatArrayConstructors);
