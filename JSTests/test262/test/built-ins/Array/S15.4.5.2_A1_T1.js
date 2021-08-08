// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Every Array object has a length property whose value is
    always a nonnegative integer less than 2^32. The value of the length property is
    numerically greater than the name of every property whose name is an array index
es5id: 15.4.5.2_A1_T1
description: Checking boundary points
---*/

//CHECK#1
var x = [];
if (x.length !== 0) {
  throw new Test262Error('#1: x = []; x.length === 0. Actual: ' + (x.length));
}

//CHECK#2
x[0] = 1;
if (x.length !== 1) {
  throw new Test262Error('#2: x = []; x[1] = 1; x.length === 1. Actual: ' + (x.length));
}

//CHECK#3
x[1] = 1;
if (x.length !== 2) {
  throw new Test262Error('#3: x = []; x[0] = 1; x[1] = 1; x.length === 2. Actual: ' + (x.length));
}

//CHECK#4
x[2147483648] = 1;
if (x.length !== 2147483649) {
  throw new Test262Error('#4: x = []; x[0] = 1; x[1] = 1; x[2147483648] = 1; x.length === 2147483649. Actual: ' + (x.length));
}

//CHECK#5
x[4294967294] = 1;
if (x.length !== 4294967295) {
  throw new Test262Error('#5: x = []; x[0] = 1; x[1] = 1; x[2147483648] = 1; x[42949672954] = 1; x.length === 4294967295. Actual: ' + (x.length));
}
