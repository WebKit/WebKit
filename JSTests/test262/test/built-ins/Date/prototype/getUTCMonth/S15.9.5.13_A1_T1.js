// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCMonth" has { DontEnum } attributes
esid: sec-date.prototype.getutcmonth
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getUTCMonth;
if (x === 1)
  Date.prototype.getUTCMonth = 2;
else
  Date.prototype.getUTCMonth = 1;
if (Date.prototype.getUTCMonth === x) {
  throw new Test262Error('#1: The Date.prototype.getUTCMonth has not the attribute ReadOnly');
}
