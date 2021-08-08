// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toUTCString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.toutcstring
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toUTCString.length;
verifyNotWritable(Date.prototype.toUTCString, "length", null, 1);
if (Date.prototype.toUTCString.length !== x) {
  throw new Test262Error('#1: The Date.prototype.toUTCString.length has the attribute ReadOnly');
}
