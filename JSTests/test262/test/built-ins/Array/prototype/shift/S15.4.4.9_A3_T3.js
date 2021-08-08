// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check ToLength(length) for non Array objects
esid: sec-array.prototype.shift
description: length is arbitrarily
---*/

var obj = {};
obj.shift = Array.prototype.shift;
obj[0] = "x";
obj[1] = "y";
obj.length = -4294967294;

//CHECK#1
var shift = obj.shift();
if (shift !== undefined) {
  throw new Test262Error('#1: var obj = {}; obj.shift = Array.prototype.shift; obj[0] = "x"; obj[1] = "y"; obj.length = -4294967294; obj.shift() === undefined. Actual: ' + (shift));
}

//CHECK#2
if (obj.length !== 0) {
  throw new Test262Error('#2: var obj = {}; obj.shift = Array.prototype.shift; obj[0] = "x"; obj[1] = "y"; obj.length = -4294967294; obj.shift(); obj.length === 0. Actual: ' + (obj.length));
}

//CHECK#3
if (obj[0] !== "x") {
  throw new Test262Error('#3: var obj = {}; obj.shift = Array.prototype.shift; obj[0] = "x"; obj[1] = "y"; obj.length = -4294967294; obj.shift(); obj[0] === "x". Actual: ' + (obj[0]));
}

//CHECK#4
if (obj[1] !== "y") {
  throw new Test262Error('#4: var obj = {}; obj.shift = Array.prototype.shift; obj[0] = "x" obj[1] = "y"; obj.length = -4294967294; obj.shift(); obj[1] === "y". Actual: ' + (obj[1]));
}
