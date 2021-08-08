// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCMinutes" has { DontEnum } attributes
esid: sec-date.prototype.getutcminutes
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('getUTCMinutes')) {
  throw new Test262Error('#1: The Date.prototype.getUTCMinutes property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "getUTCMinutes") {
    throw new Test262Error('#2: The Date.prototype.getUTCMinutes has the attribute DontEnum');
  }
}
