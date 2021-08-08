// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setHours property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.sethours
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setHours.length;
verifyNotWritable(Date.prototype.setHours, "length", null, 1);
if (Date.prototype.setHours.length !== x) {
  throw new Test262Error('#1: The Date.prototype.setHours.length has the attribute ReadOnly');
}
