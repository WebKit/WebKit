// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getday
info: The Date.prototype property "getDay" has { DontEnum } attributes
es5id: 15.9.5.16_A1_T3
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('getDay')) {
  throw new Test262Error('#1: The Date.prototype.getDay property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "getDay") {
    throw new Test262Error('#2: The Date.prototype.getDay has the attribute DontEnum');
  }
}
