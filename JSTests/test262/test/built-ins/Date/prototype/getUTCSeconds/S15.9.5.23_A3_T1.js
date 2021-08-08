// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCSeconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCSeconds.length;
verifyNotWritable(Date.prototype.getUTCSeconds, "length", null, 1);
if (Date.prototype.getUTCSeconds.length !== x) {
  throw new Test262Error('#1: The Date.prototype.getUTCSeconds.length has the attribute ReadOnly');
}
