// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: RegExp instance has no [[Construct]] internal method
es5id: 15.10.7_A2_T2
description: Checking if creating "new RegExp" instance fails
---*/

//CHECK#1
try {
  throw new Test262Error('#1.1: new new RegExp throw TypeError. Actual: ' + (new new RegExp));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#1.2: new new RegExp throw TypeError. Actual: ' + (e));
  }
}
