// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.MIN_VALUE has the attribute DontEnum
es5id: 15.7.3.3_A4
description: Checking if enumerating Number.MIN_VALUE fails
---*/

//CHECK#1
for (var x in Number) {
  if (x === "MIN_VALUE") {
    throw new Test262Error('#1: Number.MIN_VALUE has the attribute DontEnum');
  }
}

if (Number.propertyIsEnumerable('MIN_VALUE')) {
  throw new Test262Error('#2: Number.MIN_VALUE has the attribute DontEnum');
}
