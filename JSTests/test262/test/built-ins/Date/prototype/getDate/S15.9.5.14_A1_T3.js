// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getDate" has { DontEnum } attributes
esid: sec-date.prototype.getdate
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('getDate')) {
  throw new Test262Error('#1: The Date.prototype.getDate property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "getDate") {
    throw new Test262Error('#2: The Date.prototype.getDate has the attribute DontEnum');
  }
}
