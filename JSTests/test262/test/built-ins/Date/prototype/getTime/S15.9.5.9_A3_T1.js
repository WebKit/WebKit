// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getTime property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getTime.length;
verifyNotWritable(Date.prototype.getTime, "length", null, 1);
if (Date.prototype.getTime.length !== x) {
  throw new Test262Error('#1: The Date.prototype.getTime.length has the attribute ReadOnly');
}
