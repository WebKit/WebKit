// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    A property name P (in the form of a string value) is an array index
    if and only if ToString(ToUint32(P)) is equal to P and ToUint32(P) is not equal to 2^32 - 1
es5id: 15.4_A1.1_T5
description: Checking for null and undefined
---*/

//CHECK#1
var x = [];
x[null] = 0;
if (x[0] !== undefined) {
  throw new Test262Error('#1: x = []; x[null] = 1; x[0] === undefined. Actual: ' + (x[0]));
}

//CHECK#2
if (x["null"] !== 0) {
  throw new Test262Error('#2: x = []; x[null] = 1; x["null"] === 0. Actual: ' + (x["null"]));
}

//CHECK#3
var y = [];
y[undefined] = 0;
if (y[0] !== undefined) {
  throw new Test262Error('#3: y = []; y[undefined] = 0; y[0] === undefined. Actual: ' + (y[0]));
}

//CHECK#4
if (y["undefined"] !== 0) {
  throw new Test262Error('#4: y = []; y[undefined] = 1; y["undefined"] === 0. Actual: ' + (y["undefined"]));
}
