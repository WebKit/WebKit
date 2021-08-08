// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCMonth property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcmonth
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setUTCMonth.length;
verifyNotWritable(Date.prototype.setUTCMonth, "length", null, 1);
if (Date.prototype.setUTCMonth.length !== x) {
  throw new Test262Error('#1: The Date.prototype.setUTCMonth.length has the attribute ReadOnly');
}
