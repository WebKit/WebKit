// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: bitwise-and operator ToNumeric with BigInt operands
esid: sec-binary-bitwise-operators-runtime-semantics-evaluation
features: [BigInt, Symbol.toPrimitive, computed-property-names]
---*/

assert.sameValue(Object(2n) & 3n, 2n, "ToPrimitive: unbox object with internal slot");
assert.sameValue(3n & Object(2n), 2n, "ToPrimitive: unbox object with internal slot");
assert.sameValue(({
  [Symbol.toPrimitive]: function() {
    return 2n;
  }
}) & 3n, 2n, "ToPrimitive: @@toPrimitive");
assert.sameValue(3n & {
  [Symbol.toPrimitive]: function() {
    return 2n;
  }
}, 2n, "ToPrimitive: @@toPrimitive");
assert.sameValue(({
  valueOf: function() {
    return 2n;
  }
}) & 3n, 2n, "ToPrimitive: valueOf");
assert.sameValue(3n & {
  valueOf: function() {
    return 2n;
  }
}, 2n, "ToPrimitive: valueOf");
assert.sameValue(({
  toString: function() {
    return 2n;
  }
}) & 3n, 2n, "ToPrimitive: toString");
assert.sameValue(3n & {
  toString: function() {
    return 2n;
  }
}, 2n, "ToPrimitive: toString");
