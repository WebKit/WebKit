// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.gethours
info: |
    The Date.prototype.getHours property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
es5id: 15.9.5.18_A3_T3
description: Checking DontEnum attribute
---*/

if (Date.prototype.getHours.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.prototype.getHours.length property has the attribute DontEnum');
}

for (var x in Date.prototype.getHours) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.prototype.getHours.length has the attribute DontEnum');
  }
}
