// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCMinutes" has { DontEnum } attributes
esid: sec-date.prototype.setutcminutes
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setUTCMinutes;
if (x === 1)
  Date.prototype.setUTCMinutes = 2;
else
  Date.prototype.setUTCMinutes = 1;
if (Date.prototype.setUTCMinutes === x) {
  $ERROR('#1: The Date.prototype.setUTCMinutes has not the attribute ReadOnly');
}
