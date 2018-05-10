// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wake
description: >
  Allowed boundary cases for 'count' argument to Atomics.wake
info: |
  Atomics.wake( typedArray, index, count )

  ...
  3. If count is undefined, let c be +∞.
  4. Else,
    a. Let intCount be ? ToInteger(count).
  ...

  ToInteger ( argument )

  1. Let number be ? ToNumber(argument).
  2. If number is NaN, return +0.
  3. If number is +0, -0, +∞, or -∞, return number.
  4. Return the number value that is the same sign as number
      and whose magnitude is floor(abs(number)).

features: [Atomics, SharedArrayBuffer, TypedArray]
---*/

var sab = new SharedArrayBuffer(4);
var view = new Int32Array(sab);

assert.sameValue(Atomics.wake(view, 0, -3), 0);
assert.sameValue(Atomics.wake(view, 0, Number.POSITIVE_INFINITY), 0);
assert.sameValue(Atomics.wake(view, 0, undefined), 0);
assert.sameValue(Atomics.wake(view, 0, "33"), 0);
assert.sameValue(Atomics.wake(view, 0, { valueOf: 8 }), 0);
assert.sameValue(Atomics.wake(view, 0), 0);
