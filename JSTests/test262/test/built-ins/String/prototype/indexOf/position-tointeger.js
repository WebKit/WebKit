// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.indexof
description: String.prototype.indexOf type coercion for position parameter
info: >
  String.prototype.indexOf ( searchString [ , position ] )

  4. Let pos be ? ToInteger(position).

includes: [typeCoercion.js]
features: [BigInt, Symbol.toPrimitive]
---*/

testCoercibleToIntegerZero(function(zero) {
  assert.sameValue("aaaa".indexOf("aa", zero), 0);
});

testCoercibleToIntegerOne(function(one) {
  assert.sameValue("aaaa".indexOf("aa", one), 1);
});

testCoercibleToIntegerFromInteger(2, function(two) {
  assert.sameValue("aaaa".indexOf("aa", two), 2);
});

testNotCoercibleToInteger(function(error, value) {
  assert.throws(error, function() { "".indexOf("", value); });
});
