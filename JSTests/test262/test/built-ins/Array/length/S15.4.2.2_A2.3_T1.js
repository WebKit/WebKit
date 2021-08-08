// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array-len
info: |
    If the argument len is not a Number, then the length property of
    the newly constructed object is set to 1 and the 0 property of
    the newly constructed object is set to len
es5id: 15.4.2.2_A2.3_T1
description: Checking for null and undefined
---*/

var x = new Array(null);

//CHECK#1
if (x.length !== 1) {
  throw new Test262Error('#1: var x = new Array(null); x.length === 1. Actual: ' + (x.length));
}

//CHECK#2
if (x[0] !== null) {
  throw new Test262Error('#2: var x = new Array(null); x[0] === null. Actual: ' + (x[0]));
}

var x = new Array(undefined);

//CHECK#3
if (x.length !== 1) {
  throw new Test262Error('#3: var x = new Array(undefined); x.length === 1. Actual: ' + (x.length));
}

//CHECK#4
if (x[0] !== undefined) {
  throw new Test262Error('#4: var x = new Array(undefined); x[0] === undefined. Actual: ' + (x[0]));
}
