// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCMinutes property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcminutes
description: Checking DontEnum attribute
---*/

if (Date.prototype.getUTCMinutes.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.prototype.getUTCMinutes.length property has the attribute DontEnum');
}

for (var x in Date.prototype.getUTCMinutes) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.prototype.getUTCMinutes.length has the attribute DontEnum');
  }
}
