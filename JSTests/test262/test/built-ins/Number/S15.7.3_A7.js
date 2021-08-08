// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The value of the internal [[Prototype]] property of the Number
    constructor is the Function prototype object
es5id: 15.7.3_A7
description: Checking Function.prototype.isPrototypeOf(Number)
---*/

//CHECK#1
if (!(Function.prototype.isPrototypeOf(Number))) {
  throw new Test262Error('#1: the value of the internal [[Prototype]] property of the Number constructor is the Function prototype object.');
}
