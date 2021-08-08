// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getTime" has { DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getTime;
if (x === 1)
  Date.prototype.getTime = 2;
else
  Date.prototype.getTime = 1;
if (Date.prototype.getTime === x) {
  throw new Test262Error('#1: The Date.prototype.getTime has not the attribute ReadOnly');
}
