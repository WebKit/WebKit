// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.parse property "length" has { ReadOnly, DontDelete, DontEnum }
    attributes
esid: sec-date.parse
description: Checking DontEnum attribute
---*/

if (Date.parse.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: The Date.parse.length property has the attribute DontEnum');
}

for (var x in Date.parse) {
  if (x === "length") {
    throw new Test262Error('#2: The Date.parse.length has the attribute DontEnum');
  }
}
