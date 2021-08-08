// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMinutes" has { DontEnum } attributes
esid: sec-date.prototype.setminutes
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('setMinutes')) {
  throw new Test262Error('#1: The Date.prototype.setMinutes property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "setMinutes") {
    throw new Test262Error('#2: The Date.prototype.setMinutes has the attribute DontEnum');
  }
}
