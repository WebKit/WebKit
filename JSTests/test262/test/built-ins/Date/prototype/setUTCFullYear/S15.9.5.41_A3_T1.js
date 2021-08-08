// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCFullYear property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcfullyear
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setUTCFullYear.length;
verifyNotWritable(Date.prototype.setUTCFullYear, "length", null, 1);
if (Date.prototype.setUTCFullYear.length !== x) {
  throw new Test262Error('#1: The Date.prototype.setUTCFullYear.length has the attribute ReadOnly');
}
