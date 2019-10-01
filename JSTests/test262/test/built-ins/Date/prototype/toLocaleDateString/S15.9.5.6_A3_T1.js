// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toLocaleDateString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tolocaledatestring
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toLocaleDateString.length;
verifyNotWritable(Date.prototype.toLocaleDateString, "length", null, 1);
if (Date.prototype.toLocaleDateString.length !== x) {
  $ERROR('#1: The Date.prototype.toLocaleDateString.length has the attribute ReadOnly');
}
