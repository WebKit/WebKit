// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When the Object constructor is called with one argument value and
    the type of value is Boolean, return ToObject(boolean)
es5id: 15.2.2.1_A4_T3
description: Argument value is boolean expression
---*/

var n_obj = new Object((1 === 1) && !(false));

//CHECK#2
if (n_obj.constructor !== Boolean) {
  throw new Test262Error('#2: When the Object constructor is called with Boolean argument return ToObject(boolean)');
}

//CHECK#3
if (typeof n_obj !== 'object') {
  throw new Test262Error('#3: When the Object constructor is called with Boolean argument return ToObject(boolean)');
}

//CHECK#4
if (n_obj != true) {
  throw new Test262Error('#4: When the Object constructor is called with Boolean argument return ToObject(boolean)');
}

//CHECK#5
if (n_obj === true) {
  throw new Test262Error('#5: When the Object constructor is called with Boolean argument return ToObject(boolean)');
}
