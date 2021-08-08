// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When Array is called as a function rather than as a constructor,
    it creates and initialises a new Array object
es5id: 15.4.1_A3.1_T1
description: Checking use typeof, instanceof
---*/

//CHECK#1
if (typeof Array() !== "object") {
  throw new Test262Error('#1: typeof Array() === "object". Actual: ' + (typeof Array()));
}

//CHECK#2
if ((Array() instanceof Array) !== true) {
  throw new Test262Error('#2: (Array() instanceof Array) === true. Actual: ' + (Array() instanceof Array));
}
