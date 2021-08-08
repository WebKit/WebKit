// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getminutes
info: The Date.prototype property "getMinutes" has { DontEnum } attributes
es5id: 15.9.5.20_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getMinutes;
if (x === 1)
  Date.prototype.getMinutes = 2;
else
  Date.prototype.getMinutes = 1;
if (Date.prototype.getMinutes === x) {
  throw new Test262Error('#1: The Date.prototype.getMinutes has not the attribute ReadOnly');
}
