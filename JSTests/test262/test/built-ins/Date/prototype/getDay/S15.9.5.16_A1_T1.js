// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getday
info: The Date.prototype property "getDay" has { DontEnum } attributes
es5id: 15.9.5.16_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getDay;
if (x === 1)
  Date.prototype.getDay = 2;
else
  Date.prototype.getDay = 1;
if (Date.prototype.getDay === x) {
  throw new Test262Error('#1: The Date.prototype.getDay has not the attribute ReadOnly');
}
