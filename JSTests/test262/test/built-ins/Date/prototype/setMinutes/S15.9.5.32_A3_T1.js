// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setMinutes property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setminutes
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setMinutes.length;
verifyNotWritable(Date.prototype.setMinutes, "length", null, 1);
if (Date.prototype.setMinutes.length !== x) {
  throw new Test262Error('#1: The Date.prototype.setMinutes.length has the attribute ReadOnly');
}
