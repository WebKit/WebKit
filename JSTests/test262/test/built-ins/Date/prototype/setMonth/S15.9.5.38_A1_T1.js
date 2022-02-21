// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMonth" has { DontEnum } attributes
esid: sec-date.prototype.setmonth
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setMonth;
if (x === 1) {
  Date.prototype.setMonth = 2;
} else {
  Date.prototype.setMonth = 1;
}

assert.notSameValue(
  Date.prototype.setMonth,
  x,
  'The value of Date.prototype.setMonth is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
