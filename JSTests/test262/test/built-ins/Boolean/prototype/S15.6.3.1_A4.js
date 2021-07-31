// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Boolean.prototype has the attribute DontEnum
esid: sec-boolean.prototype
description: Checking if enumerating the Boolean.prototype property fails
---*/

//CHECK#1
for (x in Boolean) {
  if (x === "prototype") {
    throw new Test262Error('#1: Boolean.prototype has the attribute DontEnum');
  }
}

if (Boolean.propertyIsEnumerable('prototype')) {
  throw new Test262Error('#2: Boolean.prototype has the attribute DontEnum');
}
