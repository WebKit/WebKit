// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: modulus operator ToNumeric with BigInt operands
esid: sec-multiplicative-operators-runtime-semantics-evaluation
features: [BigInt, Symbol, Symbol.toPrimitive, computed-property-names]
---*/

assert.throws(TypeError, function() {
  Symbol("1") % 1n;
}, "ToBigInt: Symbol => TypeError");
assert.throws(TypeError, function() {
  0n % Symbol("1");
}, "ToBigInt: Symbol => TypeError");
assert.throws(TypeError, function() {
  Object(Symbol("1")) % 1n;
}, "ToBigInt: unbox object with internal slot => Symbol => TypeError");
assert.throws(TypeError, function() {
  0n % Object(Symbol("1"));
}, "ToBigInt: unbox object with internal slot => Symbol => TypeError");
assert.throws(TypeError, function() {
  ({
    [Symbol.toPrimitive]: function() {
      return Symbol("1");
    }
  }) % 1n;
}, "ToBigInt: @@toPrimitive => Symbol => TypeError");
assert.throws(TypeError, function() {
  0n % {
    [Symbol.toPrimitive]: function() {
      return Symbol("1");
    }
  };
}, "ToBigInt: @@toPrimitive => Symbol => TypeError");
assert.throws(TypeError, function() {
  ({
    valueOf: function() {
      return Symbol("1");
    }
  }) % 1n;
}, "ToBigInt: valueOf => Symbol => TypeError");
assert.throws(TypeError, function() {
  0n % {
    valueOf: function() {
      return Symbol("1");
    }
  };
}, "ToBigInt: valueOf => Symbol => TypeError");
assert.throws(TypeError, function() {
  ({
    toString: function() {
      return Symbol("1");
    }
  }) % 1n;
}, "ToBigInt: toString => Symbol => TypeError");
assert.throws(TypeError, function() {
  0n % {
    toString: function() {
      return Symbol("1");
    }
  };
}, "ToBigInt: toString => Symbol => TypeError");
