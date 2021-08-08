// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCHours property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutchours
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCHours.length;
verifyNotWritable(Date.prototype.getUTCHours, "length", null, 1);
if (Date.prototype.getUTCHours.length !== x) {
  throw new Test262Error('#1: The Date.prototype.getUTCHours.length has the attribute ReadOnly');
}
