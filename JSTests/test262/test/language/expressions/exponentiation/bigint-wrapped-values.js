// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: exponentiation operator ToNumeric with BigInt operands
esid: sec-exp-operator-runtime-semantics-evaluation
features: [BigInt, Symbol.toPrimitive, computed-property-names]
---*/

assert.sameValue(Object(2n) ** 1n, 2n, "ToPrimitive: unbox object with internal slot");
assert.sameValue(1n ** Object(2n), 1n, "ToPrimitive: unbox object with internal slot");
assert.sameValue(({
  [Symbol.toPrimitive]: function() {
    return 2n;
  }
}) ** 1n, 2n, "ToPrimitive: @@toPrimitive");
assert.sameValue(1n ** {
  [Symbol.toPrimitive]: function() {
    return 2n;
  }
}, 1n, "ToPrimitive: @@toPrimitive");
assert.sameValue(({
  valueOf: function() {
    return 2n;
  }
}) ** 1n, 2n, "ToPrimitive: valueOf");
assert.sameValue(1n ** {
  valueOf: function() {
    return 2n;
  }
}, 1n, "ToPrimitive: valueOf");
assert.sameValue(({
  toString: function() {
    return 2n;
  }
}) ** 1n, 2n, "ToPrimitive: toString");
assert.sameValue(1n ** {
  toString: function() {
    return 2n;
  }
}, 1n, "ToPrimitive: toString");
