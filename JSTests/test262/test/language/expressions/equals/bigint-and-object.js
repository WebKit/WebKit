// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Non-strict equality comparison of BigInt values and non-primitive objects
esid: sec-abstract-equality-comparison
info: |
  10. If Type(x) is either String, Number, BigInt, or Symbol and Type(y) is Object, return the result of the comparison x == ? ToPrimitive(y).
  11. If Type(x) is Object and Type(y) is either String, Number, BigInt, or Symbol, return the result of the comparison ? ToPrimitive(x) == y.

  then after the recursion:

  1. If Type(x) is the same as Type(y), then
    a. Return the result of performing Strict Equality Comparison x === y.
  ...
  6. If Type(x) is BigInt and Type(y) is String,
    a. Let n be StringToBigInt(y).
    b. If n is NaN, return false.
    c. Return the result of x == n.
  7. If Type(x) is String and Type(y) is BigInt, return the result of y == x.

features: [BigInt]
---*/

assert.sameValue(0n == Object(0n), true, "0n == Object(0n)");
assert.sameValue(Object(0n) == 0n, true, "Object(0n) == 0n");
assert.sameValue(0n == Object(1n), false, "0n == Object(1n)");
assert.sameValue(Object(1n) == 0n, false, "Object(1n) == 0n");
assert.sameValue(1n == Object(0n), false, "1n == Object(0n)");
assert.sameValue(Object(0n) == 1n, false, "Object(0n) == 1n");
assert.sameValue(1n == Object(1n), true, "1n == Object(1n)");
assert.sameValue(Object(1n) == 1n, true, "Object(1n) == 1n");
assert.sameValue(2n == Object(0n), false, "2n == Object(0n)");
assert.sameValue(Object(0n) == 2n, false, "Object(0n) == 2n");
assert.sameValue(2n == Object(1n), false, "2n == Object(1n)");
assert.sameValue(Object(1n) == 2n, false, "Object(1n) == 2n");
assert.sameValue(2n == Object(2n), true, "2n == Object(2n)");
assert.sameValue(Object(2n) == 2n, true, "Object(2n) == 2n");
assert.sameValue(0n == {}, false, "0n == {}");
assert.sameValue({} == 0n, false, "{} == 0n");
assert.sameValue(0n == {valueOf: function() { return 0n; }}, true, "0n == {valueOf: function() { return 0n; }}");
assert.sameValue({valueOf: function() { return 0n; }} == 0n, true, "{valueOf: function() { return 0n; }} == 0n");
assert.sameValue(0n == {valueOf: function() { return 1n; }}, false, "0n == {valueOf: function() { return 1n; }}");
assert.sameValue({valueOf: function() { return 1n; }} == 0n, false, "{valueOf: function() { return 1n; }} == 0n");
assert.sameValue(0n == {toString: function() { return "0"; }}, true, '0n == {toString: function() { return "0"; }}');
assert.sameValue({toString: function() { return "0"; }} == 0n, true, '{toString: function() { return "0"; }} == 0n');
assert.sameValue(0n == {toString: function() { return "1"; }}, false, '0n == {toString: function() { return "1"; }}');
assert.sameValue({toString: function() { return "1"; }} == 0n, false, '{toString: function() { return "1"; }} == 0n');
assert.sameValue(900719925474099101n == {valueOf: function() { return 900719925474099101n; }}, true, "900719925474099101n == {valueOf: function() { return 900719925474099101n; }}");
assert.sameValue({valueOf: function() { return 900719925474099101n; }} == 900719925474099101n, true, "{valueOf: function() { return 900719925474099101n; }} == 900719925474099101n");
assert.sameValue(900719925474099101n == {valueOf: function() { return 900719925474099102n; }}, false, "900719925474099101n == {valueOf: function() { return 900719925474099102n; }}");
assert.sameValue({valueOf: function() { return 900719925474099102n; }} == 900719925474099101n, false, "{valueOf: function() { return 900719925474099102n; }} == 900719925474099101n");
assert.sameValue(900719925474099101n == {toString: function() { return "900719925474099101"; }}, true, '900719925474099101n == {toString: function() { return "900719925474099101"; }}');
assert.sameValue({toString: function() { return "900719925474099101"; }} == 900719925474099101n, true, '{toString: function() { return "900719925474099101"; }} == 900719925474099101n');
assert.sameValue(900719925474099101n == {toString: function() { return "900719925474099102"; }}, false, '900719925474099101n == {toString: function() { return "900719925474099102"; }}');
assert.sameValue({toString: function() { return "900719925474099102"; }} == 900719925474099101n, false, '{toString: function() { return "900719925474099102"; }} == 900719925474099101n');
