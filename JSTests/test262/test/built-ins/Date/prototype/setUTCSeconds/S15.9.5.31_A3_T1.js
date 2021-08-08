// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCSeconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setUTCSeconds.length;
verifyNotWritable(Date.prototype.setUTCSeconds, "length", null, 1);
if (Date.prototype.setUTCSeconds.length !== x) {
  throw new Test262Error('#1: The Date.prototype.setUTCSeconds.length has the attribute ReadOnly');
}
