// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCMonth" has { DontEnum } attributes
esid: sec-date.prototype.setutcmonth
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setUTCMonth;
if (x === 1) {
  Date.prototype.setUTCMonth = 2;
} else {
  Date.prototype.setUTCMonth = 1;
}

assert.notSameValue(
  Date.prototype.setUTCMonth,
  x,
  'The value of Date.prototype.setUTCMonth is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
