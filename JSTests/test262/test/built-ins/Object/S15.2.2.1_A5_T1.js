// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When the Object constructor is called with one argument value and
    the type of value is Number, return ToObject(number)
es5id: 15.2.2.1_A5_T1
description: Argument value is any number
---*/

var num = 1.0;

//CHECK#1
if (typeof num !== 'number') {
  throw new Test262Error('#1: 1.0 is NOT a number');
}

var n_obj = new Object(num);

//CHECK#2
if (n_obj.constructor !== Number) {
  throw new Test262Error('#2: When the Object constructor is called with Number argument return ToObject(number)');
}

//CHECK#3
if (typeof n_obj !== 'object') {
  throw new Test262Error('#3: When the Object constructor is called with Number argument return ToObject(number)');
}

//CHECK#4
if (n_obj != num) {
  throw new Test262Error('#4: When the Object constructor is called with Number argument return ToObject(number)');
}

//CHECK#5
if (n_obj === num) {
  throw new Test262Error('#5: When the Object constructor is called with Number argument return ToObject(number)');
}
