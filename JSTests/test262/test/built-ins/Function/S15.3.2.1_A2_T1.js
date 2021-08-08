// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    It is permissible but not necessary to have one argument for each formal
    parameter to be specified
es5id: 15.3.2.1_A2_T1
description: >
    Values of the function constructor arguments are "arg1", "arg2",
    "arg3", "return arg1+arg2+arg3;"
---*/

//CHECK#1
try {
  var f = Function("arg1", "arg2", "arg3", "return arg1+arg2+arg3;");
} catch (e) {
  throw new Test262Error('#1: test failed');
}

//CHECK#2
if (!(f instanceof Function)) {
  throw new Test262Error('#2: It is permissible but not necessary to have one argument for each formal parameter to be specified');
}

//CHECK#3
if (f(1, 2, 3) !== 6) {
  throw new Test262Error('#3: It is permissible but not necessary to have one argument for each formal parameter to be specified');
}
