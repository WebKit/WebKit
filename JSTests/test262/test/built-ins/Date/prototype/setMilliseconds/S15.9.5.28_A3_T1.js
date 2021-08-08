// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setMilliseconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setmilliseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setMilliseconds.length;
verifyNotWritable(Date.prototype.setMilliseconds, "length", null, 1);
if (Date.prototype.setMilliseconds.length !== x) {
  throw new Test262Error('#1: The Date.prototype.setMilliseconds.length has the attribute ReadOnly');
}
