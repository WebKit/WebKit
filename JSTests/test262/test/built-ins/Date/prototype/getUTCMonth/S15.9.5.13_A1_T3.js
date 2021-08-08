// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCMonth" has { DontEnum } attributes
esid: sec-date.prototype.getutcmonth
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('getUTCMonth')) {
  throw new Test262Error('#1: The Date.prototype.getUTCMonth property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "getUTCMonth") {
    throw new Test262Error('#2: The Date.prototype.getUTCMonth has the attribute DontEnum');
  }
}
