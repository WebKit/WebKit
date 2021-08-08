// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    For every integer k that is less than the value of
    the length property of A but not less than ToUint32(length),
    if A itself has a property (not an inherited property) named ToString(k),
    then delete that property
es5id: 15.4.5.1_A1.2_T2
description: Checking an inherited property
---*/

//CHECK#1
Array.prototype[2] = -1;
var x = [0, 1, 2];
if (x[2] !== 2) {
  throw new Test262Error('#1: Array.prototype[2] = -1; x = [0,1,3]; x[2] === 2. Actual: ' + (x[2]));
}

//CHECK#2
x.length = 2;
if (x[2] !== -1) {
  throw new Test262Error('#2: Array.prototype[2] = -1; x = [0,1,3]; x.length = 2; x[2] === -1. Actual: ' + (x[2]));
}
