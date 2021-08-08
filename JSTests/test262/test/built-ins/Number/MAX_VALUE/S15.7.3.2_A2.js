// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.MAX_VALUE is ReadOnly
es5id: 15.7.3.2_A2
description: Checking if varying Number.MAX_VALUE fails
includes: [propertyHelper.js]
---*/

// CHECK#1
var x = Number.MAX_VALUE;
verifyNotWritable(Number, "MAX_VALUE", null, 1);
if (Number.MAX_VALUE !== x) {
  throw new Test262Error('#1: x = Number.MAX_VALUE; Number.MAX_VALUE = 1; Number.MAX_VALUE === x');
}
