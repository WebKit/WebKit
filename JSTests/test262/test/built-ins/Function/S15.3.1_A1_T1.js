// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The function call Function(…) is equivalent to the object creation expression
    new Function(…) with the same arguments.
es5id: 15.3.1_A1_T1
description: Create simple functions and check returned values
---*/

var f = Function("return arguments[0];");

//CHECK#1
if (!(f instanceof Function)) {
  throw new Test262Error('#1: f instanceof Function');
}

//CHECK#2
if (f(1) !== 1) {
  throw new Test262Error('#2: f(1) !== 1');
}

var g = new Function("return arguments[0];");


//CHECK#3
if (!(g instanceof Function)) {
  throw new Test262Error('#3: g instanceof Function');
}

//CHECK#4
if (g("A") !== "A") {
  throw new Test262Error('#4: g("A") !== "A"');
}

//CHECK#5
if (g("A") !== f("A")) {
  throw new Test262Error('#5: g("A") !== f("A")');
}
