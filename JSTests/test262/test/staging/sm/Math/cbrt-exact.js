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
// Properties of Math.cbrt that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assert.sameValue(Math.cbrt(NaN), NaN);

// If x is +0, the result is +0.
assert.sameValue(Math.cbrt(+0), +0);

// If x is −0, the result is −0.
assert.sameValue(Math.cbrt(-0), -0);

// If x is +∞, the result is +∞.
assert.sameValue(Math.cbrt(Infinity), Infinity);

// If x is −∞, the result is −∞.
assert.sameValue(Math.cbrt(-Infinity), -Infinity);


