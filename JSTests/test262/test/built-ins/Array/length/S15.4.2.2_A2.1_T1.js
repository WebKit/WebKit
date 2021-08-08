// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array-len
info: |
    If the argument len is a Number and ToUint32(len) is equal to len,
    then the length property of the newly constructed object is set to ToUint32(len)
es5id: 15.4.2.2_A2.1_T1
description: Array constructor is given one argument
---*/

//CHECK#1
var x = new Array(0);
if (x.length !== 0) {
  throw new Test262Error('#1: var x = new Array(0); x.length === 0. Actual: ' + (x.length));
}

//CHECK#2
var x = new Array(1);
if (x.length !== 1) {
  throw new Test262Error('#2: var x = new Array(1); x.length === 1. Actual: ' + (x.length));
}

//CHECK#3
var x = new Array(4294967295);
if (x.length !== 4294967295) {
  throw new Test262Error('#3: var x = new Array(4294967295); x.length === 4294967295. Actual: ' + (x.length));
}
