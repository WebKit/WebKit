// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: BigInt.asUintN type coercion for bits parameter
esid: pending
info: |
  BigInt.asUintN ( bits, bigint )

  1. Let bits be ? ToIndex(bits).
features: [BigInt, computed-property-names, Symbol, Symbol.toPrimitive]
---*/

assert.sameValue(BigInt.asUintN(Object(0), 1n), 0n, "ToPrimitive: unbox object with internal slot");
assert.sameValue(BigInt.asUintN({
  [Symbol.toPrimitive]: function() {
    return 0;
  }
}, 1n), 0n, "ToPrimitive: @@toPrimitive");
assert.sameValue(BigInt.asUintN({
  valueOf: function() {
    return 0;
  }
}, 1n), 0n, "ToPrimitive: valueOf");
assert.sameValue(BigInt.asUintN({
  toString: function() {
    return 0;
  }
}, 1n), 0n, "ToPrimitive: toString");
assert.sameValue(BigInt.asUintN(Object(NaN), 1n), 0n,
  "ToIndex: unbox object with internal slot => NaN => 0");
assert.sameValue(BigInt.asUintN({
  [Symbol.toPrimitive]: function() {
    return NaN;
  }
}, 1n), 0n, "ToIndex: @@toPrimitive => NaN => 0");
assert.sameValue(BigInt.asUintN({
  valueOf: function() {
    return NaN;
  }
}, 1n), 0n, "ToIndex: valueOf => NaN => 0");
assert.sameValue(BigInt.asUintN({
  toString: function() {
    return NaN;
  }
}, 1n), 0n, "ToIndex: toString => NaN => 0");
assert.sameValue(BigInt.asUintN({
  [Symbol.toPrimitive]: function() {
    return undefined;
  }
}, 1n), 0n, "ToIndex: @@toPrimitive => undefined => NaN => 0");
assert.sameValue(BigInt.asUintN({
  valueOf: function() {
    return undefined;
  }
}, 1n), 0n, "ToIndex: valueOf => undefined => NaN => 0");
assert.sameValue(BigInt.asUintN({
  toString: function() {
    return undefined;
  }
}, 1n), 0n, "ToIndex: toString => undefined => NaN => 0");
assert.sameValue(BigInt.asUintN({
  [Symbol.toPrimitive]: function() {
    return null;
  }
}, 1n), 0n, "ToIndex: @@toPrimitive => null => 0");
assert.sameValue(BigInt.asUintN({
  valueOf: function() {
    return null;
  }
}, 1n), 0n, "ToIndex: valueOf => null => 0");
assert.sameValue(BigInt.asUintN({
  toString: function() {
    return null;
  }
}, 1n), 0n, "ToIndex: toString => null => 0");
assert.sameValue(BigInt.asUintN(Object(true), 1n), 1n,
  "ToIndex: unbox object with internal slot => true => 1");
assert.sameValue(BigInt.asUintN({
  [Symbol.toPrimitive]: function() {
    return true;
  }
}, 1n), 1n, "ToIndex: @@toPrimitive => true => 1");
assert.sameValue(BigInt.asUintN({
  valueOf: function() {
    return true;
  }
}, 1n), 1n, "ToIndex: valueOf => true => 1");
assert.sameValue(BigInt.asUintN({
  toString: function() {
    return true;
  }
}, 1n), 1n, "ToIndex: toString => true => 1");
assert.sameValue(BigInt.asUintN(Object("1"), 1n), 1n,
  "ToIndex: unbox object with internal slot => parse Number");
assert.sameValue(BigInt.asUintN({
  [Symbol.toPrimitive]: function() {
    return "1";
  }
}, 1n), 1n, "ToIndex: @@toPrimitive => parse Number");
assert.sameValue(BigInt.asUintN({
  valueOf: function() {
    return "1";
  }
}, 1n), 1n, "ToIndex: valueOf => parse Number");
assert.sameValue(BigInt.asUintN({
  toString: function() {
    return "1";
  }
}, 1n), 1n, "ToIndex: toString => parse Number");
