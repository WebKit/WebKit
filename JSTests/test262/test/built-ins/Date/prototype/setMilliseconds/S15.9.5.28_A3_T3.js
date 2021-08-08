// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setMilliseconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setmilliseconds
description: Checking DontEnum attribute
---*/

if (Date.prototype.setMilliseconds.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.prototype.setMilliseconds.length property has the attribute DontEnum');
}

for (var x in Date.prototype.setMilliseconds) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.prototype.setMilliseconds.length has the attribute DontEnum');
  }
}
