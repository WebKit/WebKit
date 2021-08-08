// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCDate property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcdate
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCDate.length;
verifyNotWritable(Date.prototype.getUTCDate, "length", null, 1);
if (Date.prototype.getUTCDate.length !== x) {
  throw new Test262Error('#1: The Date.prototype.getUTCDate.length has the attribute ReadOnly');
}
