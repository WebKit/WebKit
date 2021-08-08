// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toString" has { DontEnum } attributes
esid: sec-date.prototype.tostring
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('toString')) {
  throw new Test262Error('#1: The Date.prototype.toString property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "toString") {
    throw new Test262Error('#2: The Date.prototype.toString has the attribute DontEnum');
  }
}
