// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCHours property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutchours
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setUTCHours.length;
verifyNotWritable(Date.prototype.setUTCHours, "length", null, 1);
if (Date.prototype.setUTCHours.length !== x) {
  throw new Test262Error('#1: The Date.prototype.setUTCHours.length has the attribute ReadOnly');
}
