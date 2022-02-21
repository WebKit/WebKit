// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getfullyear
info: The Date.prototype property "getFullYear" has { DontEnum } attributes
es5id: 15.9.5.10_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getFullYear;
if (x === 1) {
  Date.prototype.getFullYear = 2;
} else {
  Date.prototype.getFullYear = 1;
}

assert.notSameValue(
  Date.prototype.getFullYear,
  x,
  'The value of Date.prototype.getFullYear is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
