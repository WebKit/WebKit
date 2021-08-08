// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array-exotic-objects-defineownproperty-p-desc
info: Set the value of property length of A to Uint32(length)
es5id: 15.4.5.1_A1.3_T1
description: length is object or primitve
---*/

//CHECK#1
var x = [];
x.length = true;
if (x.length !== 1) {
  throw new Test262Error('#1: x = []; x.length = true; x.length === 1. Actual: ' + (x.length));
}

//CHECK#2
x = [0];
x.length = null;
if (x.length !== 0) {
  throw new Test262Error('#2: x = [0]; x.length = null; x.length === 0. Actual: ' + (x.length));
}

//CHECK#3
x = [0];
x.length = new Boolean(false);
if (x.length !== 0) {
  throw new Test262Error('#3: x = [0]; x.length = new Boolean(false); x.length === 0. Actual: ' + (x.length));
}

//CHECK#4
x = [];
x.length = new Number(1);
if (x.length !== 1) {
  throw new Test262Error('#4: x = []; x.length = new Number(1); x.length === 1. Actual: ' + (x.length));
}

//CHECK#5
x = [];
x.length = "1";
if (x.length !== 1) {
  throw new Test262Error('#5: x = []; x.length = "1"; x.length === 1. Actual: ' + (x.length));
}

//CHECK#6
x = [];
x.length = new String("1");
if (x.length !== 1) {
  throw new Test262Error('#6: x = []; x.length = new String("1"); x.length === 1. Actual: ' + (x.length));
}
