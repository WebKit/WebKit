// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCMilliseconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcmilliseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCMilliseconds.length;
verifyNotWritable(Date.prototype.getUTCMilliseconds, "length", null, 1);
if (Date.prototype.getUTCMilliseconds.length !== x) {
  throw new Test262Error('#1: The Date.prototype.getUTCMilliseconds.length has the attribute ReadOnly');
}
