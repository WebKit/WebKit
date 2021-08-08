// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCSeconds" has { DontEnum } attributes
esid: sec-date.prototype.getutcseconds
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getUTCSeconds;
if (x === 1)
  Date.prototype.getUTCSeconds = 2;
else
  Date.prototype.getUTCSeconds = 1;
if (Date.prototype.getUTCSeconds === x) {
  throw new Test262Error('#1: The Date.prototype.getUTCSeconds has not the attribute ReadOnly');
}
