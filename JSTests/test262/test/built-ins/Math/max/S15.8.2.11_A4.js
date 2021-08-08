// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of the Math.max method is 2
es5id: 15.8.2.11_A4
description: Checking if Math.max.length property is defined and equals to 2
---*/

// CHECK#1
if (typeof Math.max !== "function") {
  throw new Test262Error('#1: Math.max method is not defined');
}

// CHECK#2
if (typeof Math.max.length === "undefined") {
  throw new Test262Error('#2: length property of Math.max method is undefined');
}

// CHECK#3
if (Math.max.length !== 2) {
  throw new Test262Error('#3: The length property of the Math.max method is not 2');
}
