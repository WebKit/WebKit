// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getMonth property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.getmonth
description: Checking DontEnum attribute
---*/

if (Date.prototype.getMonth.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.prototype.getMonth.length property has the attribute DontEnum');
}

for (var x in Date.prototype.getMonth) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.prototype.getMonth.length has the attribute DontEnum');
  }
}
