// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Since calling Object as a function is identical to calling a function,
    list of arguments bracketing is allowed
es5id: 15.2.2.1_A6_T1
description: Creating an object with "new Object(1,2,3)"
---*/

var obj = new Object(1, 2, 3);

//CHECK#1
if (obj.constructor !== Number) {
  throw new Test262Error('#1: Since Object as a function calling is the same as function calling list of arguments can appears in braces;');
}

//CHECK#2
if (typeof obj !== "object") {
  throw new Test262Error('#2: Since Object as a function calling is the same as function calling list of arguments can appears in braces;');
}

//CHECK#3
if ((obj != 1) || (obj === 1)) {
  throw new Test262Error('3#: Since Object as a function calling is the same as function calling list of arguments can appears in braces;');
}
