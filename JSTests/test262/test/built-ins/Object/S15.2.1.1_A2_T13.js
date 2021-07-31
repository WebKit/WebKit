// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When the Object function is called with one argument value,
    and the value neither is null nor undefined, and is supplied, return ToObject(value)
es5id: 15.2.1.1_A2_T13
description: Calling Object function with boolean expression as argument value
---*/

var obj = Object((1 === 1) && (!false));

//CHECK#1
if (obj.constructor !== Boolean) {
  throw new Test262Error('#1: Object(expression) returns ToObject(expression)');
}

//CHECK#1.1
if (typeof obj !== "object") {
  throw new Test262Error('#1.1: Object(expression) returns ToObject(expression)');
}

//CHECK#2
if (!(obj)) {
  throw new Test262Error('#2: Object(expression) returns ToObject(expression)');
}

//CHECK#3
if (obj === true) {
  throw new Test262Error('#3: Object(expression) returns ToObject(expression)');
}
