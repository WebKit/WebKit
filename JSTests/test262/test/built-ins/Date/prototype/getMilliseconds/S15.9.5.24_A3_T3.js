// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getmilliseconds
info: |
    The Date.prototype.getMilliseconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
es5id: 15.9.5.24_A3_T3
description: Checking DontEnum attribute
---*/

if (Date.prototype.getMilliseconds.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.prototype.getMilliseconds.length property has the attribute DontEnum');
}

for (var x in Date.prototype.getMilliseconds) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.prototype.getMilliseconds.length has the attribute DontEnum');
  }
}
