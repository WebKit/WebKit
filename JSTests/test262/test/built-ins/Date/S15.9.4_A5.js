// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Date constructor has length property whose value is 7
esid: sec-date-constructor
description: Checking Date.length property
---*/

//CHECK#1
if (!Date.hasOwnProperty("length")) {
  throw new Test262Error('#1: Date constructor has length property');
}

//CHECK#2
if (Date.length !== 7) {
  throw new Test262Error('#2: Date constructor length property value should be 7');
}
