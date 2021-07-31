// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "toLocaleTimeString" has { DontEnum }
    attributes
esid: sec-date.prototype.tolocaletimestring
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('toLocaleTimeString')) {
  throw new Test262Error('#1: The Date.prototype.toLocaleTimeString property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "toLocaleTimeString") {
    throw new Test262Error('#2: The Date.prototype.toLocaleTimeString has the attribute DontEnum');
  }
}
