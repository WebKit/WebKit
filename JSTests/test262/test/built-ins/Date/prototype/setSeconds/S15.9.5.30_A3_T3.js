// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setSeconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setseconds
description: Checking DontEnum attribute
---*/

if (Date.prototype.setSeconds.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.prototype.setSeconds.length property has the attribute DontEnum');
}

for (var x in Date.prototype.setSeconds) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.prototype.setSeconds.length has the attribute DontEnum');
  }
}
