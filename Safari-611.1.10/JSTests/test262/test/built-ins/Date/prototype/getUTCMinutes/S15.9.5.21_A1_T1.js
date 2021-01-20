// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCMinutes" has { DontEnum } attributes
esid: sec-date.prototype.getutcminutes
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getUTCMinutes;
if (x === 1)
  Date.prototype.getUTCMinutes = 2;
else
  Date.prototype.getUTCMinutes = 1;
if (Date.prototype.getUTCMinutes === x) {
  $ERROR('#1: The Date.prototype.getUTCMinutes has not the attribute ReadOnly');
}
