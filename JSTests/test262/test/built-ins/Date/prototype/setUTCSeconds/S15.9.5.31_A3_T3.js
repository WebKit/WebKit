// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCSeconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcseconds
description: Checking DontEnum attribute
---*/

if (Date.prototype.setUTCSeconds.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.prototype.setUTCSeconds.length property has the attribute DontEnum');
}

for (var x in Date.prototype.setUTCSeconds) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.prototype.setUTCSeconds.length has the attribute DontEnum');
  }
}
