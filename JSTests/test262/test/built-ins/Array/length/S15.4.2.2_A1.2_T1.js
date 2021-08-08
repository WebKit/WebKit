// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array-len
info: The [[Class]] property of the newly constructed object is set to "Array"
es5id: 15.4.2.2_A1.2_T1
description: Checking use Object.prototype.toString
---*/

//CHECK#1
var x = new Array(0);
x.getClass = Object.prototype.toString;
if (x.getClass() !== "[object " + "Array" + "]") {
  throw new Test262Error('#1: var x = new Array(0); x.getClass = Object.prototype.toString; x is Array object. Actual: ' + (x.getClass()));
}
