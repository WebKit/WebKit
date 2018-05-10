// Copyright (C) 2018 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Return abrupt when ToInteger throws an exception on 'count' argument to Atomics.wake
info: |
  Atomics.wake( typedArray, index, count )

  ...
  3. If count is undefined, let c be +∞.
  4. Else,
    a. Let intCount be ? ToInteger(count).
  ...

features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

var sab = new SharedArrayBuffer(4);
var view = new Int32Array(sab);
var poisoned = {
  valueOf: function() {
    throw new Test262Error("should not evaluate this code");
  }
};

assert.throws(Test262Error, function() {
  Atomics.wake(view, 0, poisoned);
});
