// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.store
description: >
  Test Atomics.store on view values other than TypedArrays
includes: [testAtomics.js]
features: [ArrayBuffer, arrow-function, Atomics, DataView, for-of, let, SharedArrayBuffer]
---*/

testWithAtomicsNonViewValues(function(view) {
  assert.throws(TypeError, (() => Atomics.store(view, 0, 0)));
});
