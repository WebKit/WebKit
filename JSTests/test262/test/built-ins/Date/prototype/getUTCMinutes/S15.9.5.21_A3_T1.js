// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCMinutes property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcminutes
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCMinutes.length;
verifyNotWritable(Date.prototype.getUTCMinutes, "length", null, 1);
if (Date.prototype.getUTCMinutes.length !== x) {
  throw new Test262Error('#1: The Date.prototype.getUTCMinutes.length has the attribute ReadOnly');
}
