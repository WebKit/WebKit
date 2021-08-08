// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCFullYear" has { DontEnum } attributes
esid: sec-date.prototype.getutcfullyear
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('getUTCFullYear')) {
  throw new Test262Error('#1: The Date.prototype.getUTCFullYear property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "getUTCFullYear") {
    throw new Test262Error('#2: The Date.prototype.getUTCFullYear has the attribute DontEnum');
  }
}
