// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Test Atomics.wake on view values other than TypedArrays
includes: [testAtomics.js]
features: [SharedArrayBuffer, ArrayBuffer, DataView, Atomics, arrow-function, let, for-of]
---*/

testWithAtomicsNonViewValues(function(view) {
  assert.throws(TypeError, (() => Atomics.wake(view, 0, 0))); // Even with count == 0
});
