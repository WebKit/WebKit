// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toUTCString" has { DontEnum } attributes
esid: sec-date.prototype.toutcstring
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.toUTCString;
if (x === 1)
  Date.prototype.toUTCString = 2;
else
  Date.prototype.toUTCString = 1;
if (Date.prototype.toUTCString === x) {
  throw new Test262Error('#1: The Date.prototype.toUTCString has not the attribute ReadOnly');
}
