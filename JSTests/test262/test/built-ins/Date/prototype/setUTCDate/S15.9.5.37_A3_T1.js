// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCDate property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcdate
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setUTCDate.length;
verifyNotWritable(Date.prototype.setUTCDate, "length", null, 1);
if (Date.prototype.setUTCDate.length !== x) {
  throw new Test262Error('#1: The Date.prototype.setUTCDate.length has the attribute ReadOnly');
}
