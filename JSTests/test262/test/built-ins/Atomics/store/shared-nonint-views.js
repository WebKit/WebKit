// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.store
description: >
  Test Atomics.store on shared non-integer TypedArrays
includes: [testTypedArray.js]
features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

var buffer = new SharedArrayBuffer(1024);

testWithTypedArrayConstructors(function(TA) {
  assert.throws(TypeError, function() {
    Atomics.store(new TA(buffer), 0, 0);
  }, '`Atomics.store(new TA(buffer), 0, 0)` throws TypeError');
}, floatArrayConstructors);



