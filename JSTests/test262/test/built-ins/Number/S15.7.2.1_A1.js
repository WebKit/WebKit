// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When Number is called as part of a new expression it is
    a constructor: it initialises the newly created object
es5id: 15.7.2.1_A1
description: Checking type of the newly created object and it value
---*/

//CHECK#1
if (typeof new Number() !== "object") {
  throw new Test262Error("#1: typeof new Number() === 'object'");
}

//CHECK#2
if (new Number() === undefined) {
  throw new Test262Error("#2: new Number() should not be undefined");
}

//CHECK#3
var x3 = new Number();
if (typeof x3 !== "object") {
  throw new Test262Error("#3: typeof new Number() === 'object'");
}

//CHECK#4
var x4 = new Number();
if (x4 === undefined) {
  throw new Test262Error("#4: new Number() should not be undefined");
}

//CHECK#5
if (typeof new Number(10) !== "object") {
  throw new Test262Error("#5: typeof new Number(10) === 'object'");
}

//CHECK#6
if (new Number(10) === undefined) {
  throw new Test262Error("#6: new Number(10) should not be undefined");
}

//CHECK#7
var x7 = new Number(10);
if (typeof x7 !== "object") {
  throw new Test262Error("#7: typeof new Number(10) === 'object'");
}

//CHECK#8
var x8 = new Number(10);
if (x8 === undefined) {
  throw new Test262Error("#8: new Number(10) should not be undefined");
}
