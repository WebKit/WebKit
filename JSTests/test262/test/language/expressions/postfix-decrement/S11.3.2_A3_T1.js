// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator x-- returns x = ToNumber(x) - 1
es5id: 11.3.2_A3_T1
description: Type(x) is boolean primitive or Boolean object
---*/

//CHECK#1
var x = true; 
x--;
if (x !== 0) {
  throw new Test262Error('#1: var x = true; x--; x === 0. Actual: ' + (x));
}

//CHECK#2
var x = new Boolean(false); 
x--;
if (x !== 0 - 1) {
  throw new Test262Error('#2: var x = new Boolean(false); x--; x === 0 - 1. Actual: ' + (x));
}
