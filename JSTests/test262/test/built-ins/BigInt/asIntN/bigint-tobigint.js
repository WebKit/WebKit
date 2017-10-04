// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: pending
description: BigInt.asIntN type coercion for bigint parameter
info: >
  BigInt.asIntN ( bits, bigint )

  2. Let bigint ? ToBigInt(bigint).

features: [BigInt, Symbol, Symbol.toPrimitive]
includes: [typeCoercion.js]
---*/

testCoercibleToBigIntZero(function(zero) {
  assert.sameValue(BigInt.asIntN(2, zero), 0n);
});

testCoercibleToBigIntOne(function(one) {
  assert.sameValue(BigInt.asIntN(2, one), 1n);
});

testCoercibleToBigIntFromBigInt(10n, function(ten) {
  assert.sameValue(BigInt.asIntN(3, ten), 2n);
});

testCoercibleToBigIntFromBigInt(12345678901234567890003n, function(value) {
  assert.sameValue(BigInt.asIntN(4, value), 3n);
});

testNotCoercibleToBigInt(function(error, value) {
  assert.throws(error, function() { BigInt.asIntN(0, value); });
});
