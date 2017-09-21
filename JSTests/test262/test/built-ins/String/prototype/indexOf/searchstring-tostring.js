// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.indexof
description: String.prototype.indexOf type coercion for searchString parameter
info: >
  String.prototype.indexOf ( searchString [ , position ] )

  3. Let searchStr be ? ToString(searchString).

includes: [typeCoercion.js]
features: [Symbol.toPrimitive, BigInt]
---*/

testCoercibleToString(function(value, expectedString) {
  if (expectedString.length === 0) {
    assert.sameValue(("x_x_x").indexOf(value), 0);
  } else {
    assert.sameValue(expectedString.indexOf("\x00"), -1, "sanity check");
    assert.sameValue(("\x00\x00" + expectedString + "\x00\x00").indexOf(value), 2);
  }
});

testNotCoercibleToString(function(error, value) {
  assert.throws(error, function() { "".indexOf(value); });
});
