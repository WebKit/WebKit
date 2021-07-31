// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When the Object function is called with one argument value,
    and the value neither is null nor undefined, and is supplied, return ToObject(value)
es5id: 15.2.1.1_A2_T7
description: Calling Object function with empty string argument value
---*/

var str = '';

// CHECK#1
if (typeof(str) !== 'string') {
  throw new Test262Error('#1: "" is NOT a String');
}

var obj = Object(str);

//CHECK#2
if (obj.constructor !== String) {
  throw new Test262Error('#2: Object("") returns ToObject("")');
}

//CHECK#3
if (typeof obj !== "object") {
  throw new Test262Error('#3: Object("") returns ToObject("")');
}

//CHECK#4
if ((obj != "") || (obj === "")) {
  throw new Test262Error('#4: Object("") returns ToObject("")');
}
