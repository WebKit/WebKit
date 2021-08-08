// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    It is permissible but not necessary to have one argument for each formal
    parameter to be specified
es5id: 15.3.2.1_A2_T5
description: >
    Values of the function constructor arguments are "return"-s of
    various results and a concotenation of strings
---*/

var i = 0;

var p = {
  toString: function() {
    return "arg" + (++i)
  }
};

//CHECK#1
try {
  var f = Function(p + "," + p, p, "return arg1+arg2+arg3;");
} catch (e) {
  throw new Test262Error('#1: test failed');
}

//CHECK#2
if (!(f instanceof Function)) {
  throw new Test262Error('#2: It is permissible but not necessary to have one argument for each formal parameter to be specified');
}

//CHECK#3
if (f("", 1, 2) !== "12") {
  throw new Test262Error('#3: It is permissible but not necessary to have one argument for each formal parameter to be specified');
}
