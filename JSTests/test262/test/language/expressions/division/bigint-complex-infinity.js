// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: BigInt division of complex infinity (1/0)
esid: sec-multiplicative-operators-runtime-semantics-evaluation
info: |
  Runtime Semantics: Evaluation

  MultiplicativeExpression: MultiplicativeExpression MultiplicativeOperator ExponentiationExpression

  ...
  11. If MultiplicativeOperator is /, return T::divide(lnum, rnum).
  ...

  BigInt::divide (x, y)

  1. If y is 0n, throw a RangeError exception.
  ...
features: [BigInt]
---*/

assert.throws(RangeError, function() {
  1n / 0n
});

assert.throws(RangeError, function() {
  10n / 0n
});

assert.throws(RangeError, function() {
  0n / 0n
});

assert.throws(RangeError, function() {
  1000000000000000000n / 0n
});
