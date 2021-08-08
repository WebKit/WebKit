// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setTime" has { DontEnum } attributes
esid: sec-date.prototype.settime
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setTime;
if (x === 1)
  Date.prototype.setTime = 2;
else
  Date.prototype.setTime = 1;
if (Date.prototype.setTime === x) {
  throw new Test262Error('#1: The Date.prototype.setTime has not the attribute ReadOnly');
}
