// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of the valueOf method is 0
es5id: 15.2.4.4_A11
description: Checking the Object.prototype.valueOf.length
---*/

//CHECK#1
if (!(Object.prototype.valueOf.hasOwnProperty("length"))) {
  throw new Test262Error('#1: The length property of the toObject method is 0');
}

//CHECK#2
if (Object.prototype.valueOf.length !== 0) {
  throw new Test262Error('#2: The length property of the toObject method is 0');
}
