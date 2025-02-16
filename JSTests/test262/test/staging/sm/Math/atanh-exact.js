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
// Properties of Math.atanh that are guaranteed by the spec.

// If x is NaN, the result is NaN.
assert.sameValue(Math.atanh(NaN), NaN);

// If x is less than −1, the result is NaN.
assert.sameValue(Math.atanh(-ONE_PLUS_EPSILON), NaN);
assert.sameValue(Math.atanh(-Number.MAX_VALUE), NaN);
assert.sameValue(Math.atanh(-Infinity), NaN);

for (var i = -5; i < -1; i += 0.1)
    assert.sameValue(Math.atanh(i), NaN);

// If x is greater than 1, the result is NaN.
assert.sameValue(Math.atanh(ONE_PLUS_EPSILON), NaN);
assert.sameValue(Math.atanh(Number.MAX_VALUE), NaN);
assert.sameValue(Math.atanh(Infinity), NaN);

for (var i = +5; i > +1; i -= 0.1)
    assert.sameValue(Math.atanh(i), NaN);

// If x is −1, the result is −∞.
assert.sameValue(Math.atanh(-1), -Infinity);

// If x is +1, the result is +∞.
assert.sameValue(Math.atanh(+1), Infinity);

// If x is +0, the result is +0.
assert.sameValue(Math.atanh(+0), +0);

// If x is −0, the result is −0.
assert.sameValue(Math.atanh(-0), -0);


