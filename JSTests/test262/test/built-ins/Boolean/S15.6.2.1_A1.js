// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    When Boolean is called as part of a new expression it is
    a constructor: it initialises the newly created object
esid: sec-boolean-constructor
description: Checking type of the newly created object and it value
---*/

//CHECK#1
if (typeof new Boolean() !== "object") {
  throw new Test262Error("#1: typeof new Boolean() === 'object'");
}

//CHECK#2
if (new Boolean() === undefined) {
  throw new Test262Error("#2: new Boolean() should not be undefined");
}

//CHECK#3
var x3 = new Boolean();
if (typeof x3 !== "object") {
  throw new Test262Error("#3: typeof new Boolean() !== 'object'");
}

//CHECK#4
var x4 = new Boolean();
if (x4 === undefined) {
  throw new Test262Error("#4: new Boolean() should not be undefined");
}

//CHECK#5
if (typeof new Boolean(1) !== "object") {
  throw new Test262Error("#5: typeof new Boolean(10) === 'object'");
}

//CHECK#6
if (new Boolean(1) === undefined) {
  throw new Test262Error("#6: new Boolean(1) should not be undefined");
}

//CHECK#7
var x7 = new Boolean(1);
if (typeof x7 !== "object") {
  throw new Test262Error("#7: typeof new Boolean(1) !== 'object'");
}

//CHECK#8
var x8 = new Boolean(1);
if (x8 === undefined) {
  throw new Test262Error("#8: new Boolean(1) should not be undefined");
}
