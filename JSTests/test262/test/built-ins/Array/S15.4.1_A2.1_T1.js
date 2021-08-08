// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The length property of the newly constructed object;
    is set to the number of arguments
es5id: 15.4.1_A2.1_T1
description: Array constructor is given no arguments or at least two arguments
---*/

//CHECK#1
if (Array().length !== 0) {
  throw new Test262Error('#1: (Array().length === 0. Actual: ' + (Array().length));
}

//CHECK#2
if (Array(0, 1, 0, 1).length !== 4) {
  throw new Test262Error('#2: (Array(0,1,0,1).length === 4. Actual: ' + (Array(0, 1, 0, 1).length));
}

//CHECK#3
if (Array(undefined, undefined).length !== 2) {
  throw new Test262Error('#3: (Array(undefined, undefined).length === 2. Actual: ' + (Array(undefined, undefined).length));
}
