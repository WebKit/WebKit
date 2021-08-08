// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCMinutes property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcminutes
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setUTCMinutes.length;
verifyNotWritable(Date.prototype.setUTCMinutes, "length", null, 1);
if (Date.prototype.setUTCMinutes.length !== x) {
  throw new Test262Error('#1: The Date.prototype.setUTCMinutes.length has the attribute ReadOnly');
}
