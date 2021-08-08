// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Function constructor has length property whose value is 1
es5id: 15.3.3_A3
description: Checking Function.length property
---*/

//CHECK#1
if (!Function.hasOwnProperty("length")) {
  throw new Test262Error('#1: Function constructor has length property');
}

//CHECK#2
if (Function.length !== 1) {
  throw new Test262Error('#2: Function constructor length property value is 1');
}
