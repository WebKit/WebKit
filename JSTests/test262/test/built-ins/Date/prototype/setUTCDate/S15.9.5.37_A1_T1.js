// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCDate" has { DontEnum } attributes
esid: sec-date.prototype.setutcdate
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setUTCDate;
if (x === 1)
  Date.prototype.setUTCDate = 2;
else
  Date.prototype.setUTCDate = 1;
if (Date.prototype.setUTCDate === x) {
  throw new Test262Error('#1: The Date.prototype.setUTCDate has not the attribute ReadOnly');
}
