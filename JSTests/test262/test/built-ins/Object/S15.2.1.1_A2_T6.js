// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When the Object function is called with one argument value,
    and the value neither is null nor undefined, and is supplied, return ToObject(value)
es5id: 15.2.1.1_A2_T6
description: Calling Object function with Infinity argument value
---*/

var num = Infinity;

// CHECK#1
if (typeof num !== 'number') {
  throw new Test262Error('#1: num = Infinity should be a Number primitive');
}

var obj = Object(num);

//CHECK#2
if (obj.constructor !== Number) {
  throw new Test262Error('#2: Object(Infinity) returns ToObject(Infinity)');
}

//CHECK#3
if (typeof obj !== "object") {
  throw new Test262Error('#3: Object(Infinity) returns ToObject(Infinity)');
}

//CHECK#4
if ((obj != Infinity) || (obj === Infinity)) {
  throw new Test262Error('#4: Object(Infinity) returns ToObject(Infinity)');
}
