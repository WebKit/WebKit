// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Throws a TypeError if BigInt is called with a new target
esid: sec-bigint-constructor
info: |
  1. If NewTarget is not undefined, throw a TypeError exception.
  2. Let prim be ? ToPrimitive(value, hint Number).
  ...
features: [BigInt]
---*/

assert.throws(TypeError, function() {
  new BigInt();
});

assert.throws(TypeError, function() {
  new BigInt(NaN);
});

assert.throws(TypeError, function() {
  new BigInt({
    valueOf: function() { throw new Test262Error("unreachable"); }
  });
});

for (let x of [NaN, Infinity, 0.5, 2**53]) {
    assert.throws(RangeError, () => BigInt(x));
    assert.throws(RangeError, () => BigInt(-x));
}
assert.sameValue(BigInt(9007199254740991), 9007199254740991n);
assert.sameValue(BigInt(-9007199254740991), -9007199254740991n);
