// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toLocaleString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tolocalestring
description: Checking DontEnum attribute
---*/

if (Date.prototype.toLocaleString.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.prototype.toLocaleString.length property has the attribute DontEnum');
}

for (var x in Date.prototype.toLocaleString) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.prototype.toLocaleString.length has the attribute DontEnum');
  }
}
