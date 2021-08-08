// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "constructor" has { DontEnum } attributes
esid: sec-date.prototype.constructor
description: Checking DontEnum attribute
---*/

if (Date.prototype.propertyIsEnumerable('constructor')) {
  throw new Test262Error('#1: The Date.prototype.constructor property has the attribute DontEnum');
}

for (var x in Date.prototype) {
  if (x === "constructor") {
    throw new Test262Error('#2: The Date.prototype.constructor has the attribute DontEnum');
  }
}
