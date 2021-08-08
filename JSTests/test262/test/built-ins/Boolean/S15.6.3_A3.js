// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Boolean constructor has length property whose value is 1
esid: sec-boolean.prototype
description: Checking Boolean.length property
---*/

//CHECK#1
if (!Boolean.hasOwnProperty("length")) {
  throw new Test262Error('#1: Boolean constructor has length property');
}

//CHECK#2
if (Boolean.length !== 1) {
  throw new Test262Error('#2: Boolean constructor length property value is 1');
}
