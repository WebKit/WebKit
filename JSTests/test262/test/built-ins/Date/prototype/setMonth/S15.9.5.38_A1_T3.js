// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMonth" has { DontEnum } attributes
esid: sec-date.prototype.setmonth
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('setMonth')) {
  throw new Test262Error('#1: The Date.prototype.setMonth property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "setMonth") {
    throw new Test262Error('#2: The Date.prototype.setMonth has the attribute DontEnum');
  }
}
