// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCMonth property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcmonth
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCMonth.length;
verifyNotWritable(Date.prototype.getUTCMonth, "length", null, 1);
if (Date.prototype.getUTCMonth.length !== x) {
  throw new Test262Error('#1: The Date.prototype.getUTCMonth.length has the attribute ReadOnly');
}
