// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If P is not an array index, return
    (Create a property with name P, set its value to V and give it empty attributes)
es5id: 15.4.5.1_A2.1_T1
description: P in [4294967295, -1, true]
---*/

//CHECK#1
var x = [];
x[4294967295] = 1;
if (x.length !== 0) {
  throw new Test262Error('#1.1: x = []; x[4294967295] = 1; x.length === 0. Actual: ' + (x.length));
}

if (x[4294967295] !== 1) {
  throw new Test262Error('#1.2: x = []; x[4294967295] = 1; x[4294967295] === 1. Actual: ' + (x[4294967295]));
}

//CHECK#2
x = [];
x[-1] = 1;
if (x.length !== 0) {
  throw new Test262Error('#2.1: x = []; x[-1] = 1; x.length === 0. Actual: ' + (x.length));
}

if (x[-1] !== 1) {
  throw new Test262Error('#2.2: x = []; x[-1] = 1; x[-1] === 1. Actual: ' + (x[-1]));
}

//CHECK#3
x = [];
x[true] = 1;
if (x.length !== 0) {
  throw new Test262Error('#3.1: x = []; x[true] = 1; x.length === 0. Actual: ' + (x.length));
}

if (x[true] !== 1) {
  throw new Test262Error('#3.2: x = []; x[true] = 1; x[true] === 1. Actual: ' + (x[true]));
}
