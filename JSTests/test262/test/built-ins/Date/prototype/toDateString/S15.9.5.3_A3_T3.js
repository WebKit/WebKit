// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toDateString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.todatestring
description: Checking DontEnum attribute
---*/

if (Date.prototype.toDateString.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.prototype.toDateString.length property has the attribute DontEnum');
}

for (var x in Date.prototype.toDateString) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.prototype.toDateString.length has the attribute DontEnum');
  }
}
