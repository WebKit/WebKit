// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.valueOf property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.valueof
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.valueOf.length;
verifyNotWritable(Date.prototype.valueOf, "length", null, 1);
if (Date.prototype.valueOf.length !== x) {
  throw new Test262Error('#1: The Date.prototype.valueOf.length has the attribute ReadOnly');
}
