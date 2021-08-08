// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getminutes
info: The Date.prototype property "getMinutes" has { DontEnum } attributes
es5id: 15.9.5.20_A1_T3
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('getMinutes')) {
  throw new Test262Error('#1: The Date.prototype.getMinutes property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "getMinutes") {
    throw new Test262Error('#2: The Date.prototype.getMinutes has the attribute DontEnum');
  }
}
