// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Every Array object has a length property whose value is
    always a nonnegative integer less than 2^32. The value of the length property is
    numerically greater than the name of every property whose name is an array index
es5id: 15.4.5.2_A1_T2
description: P = "2^32 - 1" is not index array
---*/

//CHECK#1
var x = [];
x[4294967295] = 1;
if (x.length !== 0) {
  throw new Test262Error('#1: x = []; x[4294967295] = 1; x.length === 0. Actual: ' + (x.length));
}

//CHECK#2
var y = [];
y[1] = 1;
y[4294967295] = 1;
if (y.length !== 2) {
  throw new Test262Error('#2: y = []; y[1] = 1; y[4294967295] = 1; y.length === 2. Actual: ' + (y.length));
}
