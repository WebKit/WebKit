// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setSeconds" has { DontEnum } attributes
esid: sec-date.prototype.setseconds
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setSeconds;
if (x === 1)
  Date.prototype.setSeconds = 2;
else
  Date.prototype.setSeconds = 1;
if (Date.prototype.setSeconds === x) {
  throw new Test262Error('#1: The Date.prototype.setSeconds has not the attribute ReadOnly');
}
