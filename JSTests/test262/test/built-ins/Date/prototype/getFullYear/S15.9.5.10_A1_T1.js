// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getfullyear
info: The Date.prototype property "getFullYear" has { DontEnum } attributes
es5id: 15.9.5.10_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getFullYear;
if (x === 1)
  Date.prototype.getFullYear = 2;
else
  Date.prototype.getFullYear = 1;
if (Date.prototype.getFullYear === x) {
  throw new Test262Error('#1: The Date.prototype.getFullYear has not the attribute ReadOnly');
}
