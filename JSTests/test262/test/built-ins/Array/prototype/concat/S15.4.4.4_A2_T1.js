// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.concat
info: |
    The concat function is intentionally generic.
    It does not require that its this value be an Array object
es5id: 15.4.4.4_A2_T1
description: Checking this for Object object, items are objects and primitives
---*/

var x = {};
x.concat = Array.prototype.concat;
var y = new Object();
var z = new Array(1, 2);
var arr = x.concat(y, z, -1, true, "NaN");

//CHECK#0
arr.getClass = Object.prototype.toString;
if (arr.getClass() !== "[object " + "Array" + "]") {
  throw new Test262Error('#0: var x = {}; x.concat = Array.prototype.concat; var y = new Object(); var z = new Array(1,2); var arr = x.concat(y,z, -1, true, "NaN"); arr is Array object. Actual: ' + (arr.getClass()));
}

//CHECK#1
if (arr[0] !== x) {
  throw new Test262Error('#1: var x = {}; x.concat = Array.prototype.concat; var y = new Object(); var z = new Array(1,2); var arr = x.concat(y,z, -1, true, "NaN"); arr[0] === x. Actual: ' + (arr[0]));
}

//CHECK#2
if (arr[1] !== y) {
  throw new Test262Error('#2: var x = {}; x.concat = Array.prototype.concat; var y = new Object(); var z = new Array(1,2); var arr = x.concat(y,z, -1, true, "NaN"); arr[1] === y. Actual: ' + (arr[1]));
}

//CHECK#3
if (arr[2] !== 1) {
  throw new Test262Error('#3: var x = {}; x.concat = Array.prototype.concat; var y = new Object(); var z = new Array(1,2); var arr = x.concat(y,z, -1, true, "NaN"); arr[2] === 1. Actual: ' + (arr[2]));
}

//CHECK#4
if (arr[3] !== 2) {
  throw new Test262Error('#4: var x = {}; x.concat = Array.prototype.concat; var y = new Object(); var z = new Array(1,2); var arr = x.concat(y,z, -1, true, "NaN"); arr[3] === 2. Actual: ' + (arr[3]));
}

//CHECK#5
if (arr[4] !== -1) {
  throw new Test262Error('#5: var x = {}; x.concat = Array.prototype.concat; var y = new Object(); var z = new Array(1,2); var arr = x.concat(y,z, -1, true, "NaN"); arr[4] === -1. Actual: ' + (arr[4]));
}

//CHECK#6
if (arr[5] !== true) {
  throw new Test262Error('#6: var x = {}; x.concat = Array.prototype.concat; var y = new Object(); var z = new Array(1,2); var arr = x.concat(y,z, -1, true, "NaN"); arr[5] === true. Actual: ' + (arr[5]));
}

//CHECK#7
if (arr[6] !== "NaN") {
  throw new Test262Error('#7: var x = {}; x.concat = Array.prototype.concat; var y = new Object(); var z = new Array(1,2); var arr = x.concat(y,z, -1, true, "NaN"); arr[6] === "NaN". Actual: ' + (arr[6]));
}

//CHECK#8
if (arr.length !== 7) {
  throw new Test262Error('#8: var x = {}; x.concat = Array.prototype.concat; var y = new Object(); var z = new Array(1,2); var arr = x.concat(y,z, -1, true, "NaN"); arr.length === 7. Actual: ' + (arr.length));
}
