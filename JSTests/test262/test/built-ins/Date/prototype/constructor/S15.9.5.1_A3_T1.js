// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.constructor property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.constructor
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.constructor.length;
verifyNotWritable(Date.prototype.constructor, "length", null, 1);
if (Date.prototype.constructor.length !== x) {
  throw new Test262Error('#1: The Date.prototype.constructor.length has the attribute ReadOnly');
}
