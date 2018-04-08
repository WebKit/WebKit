// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.sub
description: >
  Test Atomics.sub on view values other than TypedArrays
includes: [testAtomics.js]
features: [SharedArrayBuffer, ArrayBuffer, DataView, Atomics, arrow-function, let, for-of]
---*/

testWithAtomicsNonViewValues(function(view) {
  assert.throws(TypeError, (() => Atomics.sub(view, 0, 0)));
});
