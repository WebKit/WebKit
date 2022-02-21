// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getMonth" has { DontEnum } attributes
esid: sec-date.prototype.getmonth
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getMonth;
if (x === 1) {
  Date.prototype.getMonth = 2;
} else {
  Date.prototype.getMonth = 1;
}

assert.notSameValue(
  Date.prototype.getMonth,
  x,
  'The value of Date.prototype.getMonth is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
