// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: pending
description: BigInt.asUintN type coercion for bits parameter
info: >
  BigInt.asUintN ( bits, bigint )

  1. Let bits be ? ToIndex(bits).

features: [BigInt, Symbol, Symbol.toPrimitive]
includes: [typeCoercion.js]
---*/

testCoercibleToIndexZero(function(zero) {
  assert.sameValue(BigInt.asUintN(zero, 1n), 0n);
});

testCoercibleToIndexOne(function(one) {
  assert.sameValue(BigInt.asUintN(one, 1n), 1n);
});

testCoercibleToIndexFromIndex(3, function(three) {
  assert.sameValue(BigInt.asUintN(three, 10n), 2n);
});

testNotCoercibleToIndex(function(error, value) {
  assert.throws(error, function() { BigInt.asUintN(value, 0n); });
});
