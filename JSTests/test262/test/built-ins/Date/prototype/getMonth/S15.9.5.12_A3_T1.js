// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getMonth property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.getmonth
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getMonth.length;
verifyNotWritable(Date.prototype.getMonth, "length", null, 1);
if (Date.prototype.getMonth.length !== x) {
  throw new Test262Error('#1: The Date.prototype.getMonth.length has the attribute ReadOnly');
}
