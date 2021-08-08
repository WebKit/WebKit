// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The value of the internal [[Prototype]] property of the Date
    constructor is the Function prototype object
esid: sec-date-constructor
description: Checking Function.prototype.isPrototypeOf(Date)
---*/

//CHECK#1
if (!(Function.prototype.isPrototypeOf(Date))) {
  throw new Test262Error('#1: the value of the internal [[Prototype]] property of the Date constructor is the Function prototype object.');
}
