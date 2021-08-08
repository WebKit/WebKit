// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setDate" has { DontEnum } attributes
esid: sec-date.prototype.setdate
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('setDate')) {
  throw new Test262Error('#1: The Date.prototype.setDate property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "setDate") {
    throw new Test262Error('#2: The Date.prototype.setDate has the attribute DontEnum');
  }
}
