// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCDate property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcdate
description: Checking DontEnum attribute
---*/

if (Date.prototype.setUTCDate.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.prototype.setUTCDate.length property has the attribute DontEnum');
}

for (var x in Date.prototype.setUTCDate) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.prototype.setUTCDate.length has the attribute DontEnum');
  }
}
