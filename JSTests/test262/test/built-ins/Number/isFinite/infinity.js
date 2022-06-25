// Copyright (C) 2016 The V8 Project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-number.isfinite
description: >
  Return false if argument is Infinity or -Infinity
info: |
  Number.isFinite ( number )

  [...]
  2. If number is NaN, +∞, or -∞, return false.
  [...]
---*/

assert.sameValue(Number.isFinite(Infinity), false, "+Infinity");
assert.sameValue(Number.isFinite(-Infinity), false, "-Infinity");
