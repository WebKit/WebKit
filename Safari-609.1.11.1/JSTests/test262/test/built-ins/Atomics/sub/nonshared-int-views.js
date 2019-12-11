// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.sub
description: >
  Test Atomics.sub on non-shared integer TypedArrays
includes: [testTypedArray.js]
features: [ArrayBuffer, Atomics, TypedArray]
---*/

const buffer = new ArrayBuffer(16);
const views = intArrayConstructors.slice();

testWithTypedArrayConstructors(function(TA) {
  assert.throws(TypeError, function() {
    Atomics.sub(new TA(buffer), 0, 0);
  }, '`Atomics.sub(new TA(buffer), 0, 0)` throws TypeError');
}, views);
