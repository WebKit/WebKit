// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: If the BigInt exponent is < 0, throw a RangeError exception
esid: sec-exp-operator-runtime-semantics-evaluation
info: |
  ExponentiationExpression: UpdateExpression ** ExponentiationExpression

  ...
  9. Return ? Type(base)::exponentiate(base, exponent).

  BigInt::exponentiate (base, exponent)

  1. If exponent < 0, throw a RangeError exception.
  2. If base is 0n and exponent is 0n, return 1n.
  3. Return a BigInt representing the mathematical value of base raised to the power exponent.
  ...
features: [BigInt]
---*/

assert.throws(RangeError, function() {
  1n ** -1n
});

assert.throws(RangeError, function() {
  0n ** -1n
});

assert.throws(RangeError, function() {
  (-1n) ** -1n
});

assert.throws(RangeError, function() {
  1n ** -100000000000000000n
});
