// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check ToLength(length) for Array object
esid: sec-array.prototype.push
description: If ToUint32(length) !== length, throw RangeError
---*/

var x = [];
x.length = 4294967295;

//CHECK#1
var push = x.push();
if (push !== 4294967295) {
  throw new Test262Error('#1: x = []; x.length = 4294967295; x.push() === 4294967295. Actual: ' + (push));
}

//CHECK#2
try {
  x.push("x");
  throw new Test262Error('#2.1: x = []; x.length = 4294967295; x.push("x") throw RangeError. Actual: ' + (push));
} catch (e) {
  if ((e instanceof RangeError) !== true) {
    throw new Test262Error('#2.2: x = []; x.length = 4294967295; x.push("x") throw RangeError. Actual: ' + (e));
  }
}

//CHECK#3
if (x[4294967295] !== "x") {
  throw new Test262Error('#3: x = []; x.length = 4294967295; try {x.push("x")}catch(e){}; x[4294967295] === "x". Actual: ' + (x[4294967295]));
}

//CHECK#4
if (x.length !== 4294967295) {
  throw new Test262Error('#4: x = []; x.length = 4294967295; try {x.push("x")}catch(e){}; x.length === 4294967295. Actual: ' + (x.length));
}
