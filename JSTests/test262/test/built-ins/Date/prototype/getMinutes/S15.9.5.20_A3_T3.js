// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getminutes
info: |
    The Date.prototype.getMinutes property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
es5id: 15.9.5.20_A3_T3
description: Checking DontEnum attribute
---*/

if (Date.prototype.getMinutes.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.prototype.getMinutes.length property has the attribute DontEnum');
}

for (var x in Date.prototype.getMinutes) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.prototype.getMinutes.length has the attribute DontEnum');
  }
}
