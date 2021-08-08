// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setTime property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.settime
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setTime.length;
verifyNotWritable(Date.prototype.setTime, "length", null, 1);
if (Date.prototype.setTime.length !== x) {
  throw new Test262Error('#1: The Date.prototype.setTime.length has the attribute ReadOnly');
}
