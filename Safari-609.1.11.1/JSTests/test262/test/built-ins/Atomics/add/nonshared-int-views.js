// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.add
description: >
  Test Atomics.add on non-shared integer TypedArrays
includes: [testTypedArray.js]
features: [ArrayBuffer, Atomics, TypedArray]
---*/

const ab = new ArrayBuffer(16);
const views = intArrayConstructors.slice();

testWithTypedArrayConstructors(function(TA) {
  assert.throws(TypeError, function() {
    Atomics.add(new TA(ab), 0, 0);
  }, '`Atomics.add(new TA(ab), 0, 0)` throws TypeError');
}, views);
