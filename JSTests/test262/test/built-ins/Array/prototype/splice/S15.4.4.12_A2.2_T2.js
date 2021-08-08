// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToInteger from deleteCount
esid: sec-array.prototype.splice
description: deleteCount = NaN
---*/

var x = [0, 1];
var arr = x.splice(0, NaN);

//CHECK#0
arr.getClass = Object.prototype.toString;
if (arr.getClass() !== "[object " + "Array" + "]") {
  throw new Test262Error('#0: var x = [0,1]; var arr = x.splice(0,NaN); arr is Array object. Actual: ' + (arr.getClass()));
}

//CHECK#1
if (arr.length !== 0) {
  throw new Test262Error('#1: var x = [0,1]; var arr = x.splice(0,NaN); arr.length === 0. Actual: ' + (arr.length));
}

//CHECK#2
if (x.length !== 2) {
  throw new Test262Error('#2: var x = [0,1]; var arr = x.splice(0,NaN); x.length === 2. Actual: ' + (x.length));
}

//CHECK#3
if (x[0] !== 0) {
  throw new Test262Error('#3: var x = [0,1]; var arr = x.splice(0,NaN); x[0] === 0. Actual: ' + (x[0]));
}

//CHECK#4
if (x[1] !== 1) {
  throw new Test262Error('#4: var x = [0,1]; var arr = x.splice(0,NaN); x[1] === 1. Actual: ' + (x[1]));
}
