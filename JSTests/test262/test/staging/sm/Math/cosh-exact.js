// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-Math-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Properties of Math.cosh that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assert.sameValue(Math.cosh(NaN), NaN);

// If x is +0, the result is 1.
assert.sameValue(Math.cosh(+0), 1);

// If x is −0, the result is 1.
assert.sameValue(Math.cosh(-0), 1);

// If x is +∞, the result is +∞.
assert.sameValue(Math.cosh(Infinity), Infinity);

// If x is −∞, the result is +∞.
assert.sameValue(Math.cosh(-Infinity), Infinity);


