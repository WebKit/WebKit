// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When the Object function is called with one argument value,
    and the value neither is null nor undefined, and is supplied, return ToObject(value)
es5id: 15.2.1.1_A2_T11
description: Calling Object function with function declaration as argument value
---*/

//CHECK#1
if (typeof func !== 'undefined') {
  throw new Test262Error('#1:  function expression can\'t be declarated');
}

var n_obj = Object(function func() {
  return 1;
});

//CHECK#2
if ((n_obj.constructor !== Function) || (n_obj() !== 1)) {
  throw new Test262Error('#2: Object(function func(){return 1;}) returns function');
}

//CHECK#3
if (typeof func !== 'undefined') {
  throw new Test262Error('#3:  function expression can\'t be declarated');
}
