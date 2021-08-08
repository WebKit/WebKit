// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toLocaleString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tolocalestring
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toLocaleString.length;
verifyNotWritable(Date.prototype.toLocaleString, "length", null, 1);
if (Date.prototype.toLocaleString.length !== x) {
  throw new Test262Error('#1: The Date.prototype.toLocaleString.length has the attribute ReadOnly');
}
