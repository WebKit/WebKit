// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toDateString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.todatestring
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toDateString.length;
verifyNotWritable(Date.prototype.toDateString, "length", null, 1);
if (Date.prototype.toDateString.length !== x) {
  throw new Test262Error('#1: The Date.prototype.toDateString.length has the attribute ReadOnly');
}
