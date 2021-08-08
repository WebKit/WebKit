// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Since calling Object as a function is identical to calling a function,
    list of arguments bracketing is allowed
es5id: 15.2.1.1_A3_T2
description: Creating an object with "Object(null,2,3)"
---*/

var obj = Object(null, 2, 3);

//CHECK#1
if (obj.constructor !== Object) {
  throw new Test262Error('#1: Since Object as a function calling is the same as function calling list of arguments can appears in braces;');
}

//CHECK#2
if (typeof obj !== "object") {
  throw new Test262Error('#2: Since Object as a function calling is the same as function calling list of arguments can appears in braces;');
}
