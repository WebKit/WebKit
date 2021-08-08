// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCHours" has { DontEnum } attributes
esid: sec-date.prototype.setutchours
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('setUTCHours')) {
  throw new Test262Error('#1: The Date.prototype.setUTCHours property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "setUTCHours") {
    throw new Test262Error('#2: The Date.prototype.setUTCHours has the attribute DontEnum');
  }
}
