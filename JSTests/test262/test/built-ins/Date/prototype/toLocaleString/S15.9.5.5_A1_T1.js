// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toLocaleString" has { DontEnum } attributes
esid: sec-date.prototype.tolocalestring
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.toLocaleString;
if (x === 1) {
  Date.prototype.toLocaleString = 2;
} else {
  Date.prototype.toLocaleString = 1;
}

assert.notSameValue(
  Date.prototype.toLocaleString,
  x,
  'The value of Date.prototype.toLocaleString is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
