// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    A property name P (in the form of a string value) is an array index
    if and only if ToString(ToUint32(P)) is equal to P and ToUint32(P) is not equal to 2^32 - 1
es5id: 15.4_A1.1_T6
description: Checking for Boolean object
---*/

//CHECK#1
var x = [];
x[new Boolean(true)] = 1;
if (x[1] !== undefined) {
  throw new Test262Error('#1: x = []; x[new Boolean(true)] = 1; x[1] === undefined. Actual: ' + (x[1]));
}

//CHECK#2
if (x["true"] !== 1) {
  throw new Test262Error('#2: x = []; x[true] = 1; x["true"] === 1. Actual: ' + (x["true"]));
}

//CHECK#3
x[new Boolean(false)] = 0;
if (x[0] !== undefined) {
  throw new Test262Error('#3: x = []; x[true] = 1; x[new Boolean(false)] = 0; x[0] === undefined. Actual: ' + (x[0]));
}

//CHECK#4
if (x["false"] !== 0) {
  throw new Test262Error('#4: x = []; x[false] = 1; x["false"] === 0. Actual: ' + (x["false"]));
}
