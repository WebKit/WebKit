// Copyright (C) 2018 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  NaNs are converted to 0 for 'count' argument to Atomics.wake
info: |
  Atomics.wake( typedArray, index, count )

  ...
  3. If count is undefined, let c be +∞.
  4. Else,
    a. Let intCount be ? ToInteger(count).
  ...

  ToInteger ( argument )

  ...
  2. If number is NaN, return +0.
  ...

features: [Atomics, SharedArrayBuffer, TypedArray]
includes: [nans.js]
---*/

var sab = new SharedArrayBuffer(4);
var view = new Int32Array(sab);

NaNs.forEach(nan => {
  assert.sameValue(Atomics.wake(view, 0, nan), 0);
});
