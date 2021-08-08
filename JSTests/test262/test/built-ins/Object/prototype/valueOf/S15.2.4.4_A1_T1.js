// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The valueOf method returns its "this" value
es5id: 15.2.4.4_A1_T1
description: "\"this\" value is a number"
---*/

//CHECK#1
if (typeof Object.prototype.valueOf !== "function") {
  throw new Test262Error('#1: valueOf method defined');
}

var obj = new Object(1.1);

//CHECK#2
if (typeof obj.valueOf !== "function") {
  throw new Test262Error('#2: valueOf method accessed');
}

//CHECK#3
if (obj.valueOf() !== 1.1) {
  throw new Test262Error('#3: The valueOf method returns its this value');
}
