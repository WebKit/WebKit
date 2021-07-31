// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When the Object function is called with one argument value,
    and the value neither is null nor undefined, and is supplied, return ToObject(value)
es5id: 15.2.1.1_A2_T1
description: Calling Object function with boolean argument value
---*/

var bool = true;

if (typeof bool !== 'boolean') {
  throw new Test262Error('#1: bool should be boolean primitive');
}

var obj = Object(bool);

if (obj.constructor !== Boolean) {
  throw new Test262Error('#2: Object(true) returns ToObject(true)');
}

if (typeof obj !== "object") {
  throw new Test262Error('#3: Object(true) returns ToObject(true)');
}

if (!obj) {
  throw new Test262Error('#4: Object(true) returns ToObject(true)');
}

if (obj === true) {
  throw new Test262Error('#5: Object(true) returns ToObject(true)');
}
