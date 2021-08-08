// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    A property name P (in the form of a string value) is an array index
    if and only if ToString(ToUint32(P)) is equal to P and ToUint32(P) is not equal to 2^32 - 1
es5id: 15.4_A1.1_T8
description: Checking for Number object
---*/

//CHECK#1
var x = [];
x[new String("0")] = 0;
if (x[0] !== 0) {
  throw new Test262Error('#1: x = []; x[new String("0")] = 0; x[0] === 0. Actual: ' + (x[0]));
}

//CHECK#2
var y = [];
y[new String("1")] = 1;
if (y[1] !== 1) {
  throw new Test262Error('#2: y = []; y[new String("1")] = 1; y[1] === 1. Actual: ' + (y[1]));
}

//CHECK#3
var z = [];
z[new String("1.1")] = 1;
if (z["1.1"] !== 1) {
  throw new Test262Error('#3: z = []; z[new String("1.1")] = 1; z["1.1"] === 1. Actual: ' + (z["1.1"]));
}
