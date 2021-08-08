// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "getTimezoneOffset" has { DontEnum }
    attributes
esid: sec-date.prototype.gettimezoneoffset
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getTimezoneOffset;
if (x === 1)
  Date.prototype.getTimezoneOffset = 2;
else
  Date.prototype.getTimezoneOffset = 1;
if (Date.prototype.getTimezoneOffset === x) {
  throw new Test262Error('#1: The Date.prototype.getTimezoneOffset has not the attribute ReadOnly');
}
