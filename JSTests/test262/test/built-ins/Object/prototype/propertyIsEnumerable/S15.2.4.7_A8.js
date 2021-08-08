// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Object.prototype.propertyIsEnumerable.length property has the
    attribute DontEnum
es5id: 15.2.4.7_A8
description: >
    Checking if enumerating the
    Object.prototype.propertyIsEnumerable.length property fails
---*/

//CHECK#0
if (!(Object.prototype.propertyIsEnumerable.hasOwnProperty('length'))) {
  throw new Test262Error('#0: the Object.prototype.propertyIsEnumerable has length property');
}


// CHECK#1
if (Object.prototype.propertyIsEnumerable.propertyIsEnumerable('length')) {
  throw new Test262Error('#1: the Object.prototype.propertyIsEnumerable.length property has the attributes DontEnum');
}

// CHECK#2
for (var p in Object.prototype.propertyIsEnumerable) {
  if (p === "length")
    throw new Test262Error('#2: the Object.prototype.propertyIsEnumerable.length property has the attributes DontEnum');
}
//
