// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMinutes" has { DontEnum } attributes
esid: sec-date.prototype.setminutes
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setMinutes;
if (x === 1)
  Date.prototype.setMinutes = 2;
else
  Date.prototype.setMinutes = 1;
if (Date.prototype.setMinutes === x) {
  $ERROR('#1: The Date.prototype.setMinutes has not the attribute ReadOnly');
}
