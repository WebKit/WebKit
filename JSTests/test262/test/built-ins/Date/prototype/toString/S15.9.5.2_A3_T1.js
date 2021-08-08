// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toString property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.tostring
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toString.length;
verifyNotWritable(Date.prototype.toString, "length", null, 1);
if (Date.prototype.toString.length !== x) {
  throw new Test262Error('#1: The Date.prototype.toString.length has the attribute ReadOnly');
}
