// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setTime" has { DontEnum } attributes
esid: sec-date.prototype.settime
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('setTime')) {
  throw new Test262Error('#1: The Date.prototype.setTime property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "setTime") {
    throw new Test262Error('#2: The Date.prototype.setTime has the attribute DontEnum');
  }
}
