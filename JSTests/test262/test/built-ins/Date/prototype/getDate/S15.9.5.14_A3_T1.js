// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getDate property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.getdate
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getDate.length;
verifyNotWritable(Date.prototype.getDate, "length", null, 1);
if (Date.prototype.getDate.length !== x) {
  throw new Test262Error('#1: The Date.prototype.getDate.length has the attribute ReadOnly');
}
