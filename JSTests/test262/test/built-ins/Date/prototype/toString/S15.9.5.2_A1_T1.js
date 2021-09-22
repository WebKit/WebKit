// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toString" has { DontEnum } attributes
esid: sec-date.prototype.tostring
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.toString;
if (x === 1) {
  Date.prototype.toString = 2;
} else {
  Date.prototype.toString = 1;
}

assert.notSameValue(
  Date.prototype.toString,
  x,
  'The value of Date.prototype.toString is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
