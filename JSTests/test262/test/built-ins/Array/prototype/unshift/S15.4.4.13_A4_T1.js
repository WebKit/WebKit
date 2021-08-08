// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "[[Get]], [[Delete]] from not an inherited property"
esid: sec-array.prototype.unshift
description: >
    [[Prototype]] of Array instance is Array.prototype, [[Prototype]
    of Array.prototype is Object.prototype
---*/

Array.prototype[0] = -1;
var x = [1];
x.length = 1;

//CHECK#1
var unshift = x.unshift(0);
if (unshift !== 2) {
  throw new Test262Error('#1: Array.prototype[0] = -1; x = [1]; x.length = 1; x.unshift(0) === 2. Actual: ' + (unshift));
}

//CHECK#2
if (x[0] !== 0) {
  throw new Test262Error('#2: Array.prototype[0] = -1; x = [1]; x.length = 1; x.unshift(0); x[0] === 0. Actual: ' + (x[0]));
}

//CHECK#3
if (x[1] !== 1) {
  throw new Test262Error('#3: Array.prototype[0] = -1; x = [1]; x.length = 1; x.unshift(0); x[1] === 1. Actual: ' + (x[1]));
}

delete x[0];

//CHECK#4
if (x[0] !== -1) {
  throw new Test262Error('#4: Array.prototype[0] = -1; x = [1]; x.length = 1; x.unshift(0); delete x[0]; x[0] === -1. Actual: ' + (x[0]));
}

Object.prototype[0] = -1;
Object.prototype.length = 1;
Object.prototype.unshift = Array.prototype.unshift;
x = {
  0: 1
};

//CHECK#5
var unshift = x.unshift(0);
if (unshift !== 2) {
  throw new Test262Error('#5: Object.prototype[0] = -1; Object.prototype.length = 1; Object.prototype.unshift = Array.prototype.unshift; x = {0:0}; x.unshift(0) === 2. Actual: ' + (unshift));
}

//CHECK#6
if (x[0] !== 0) {
  throw new Test262Error('#6: Object.prototype[0] = -1; Object.prototype.length = 1; Object.prototype.unshift = Array.prototype.unshift; x = {0:0}; x.unshift(0); x[0] === 0. Actual: ' + (x[0]));
}

//CHECK#7
if (x[1] !== 1) {
  throw new Test262Error('#7: Object.prototype[0] = -1; Object.prototype.length = 1; Object.prototype.unshift = Array.prototype.unshift; x = {0:0}; x.unshift(0); x[1] === 1. Actual: ' + (x[1]));
}

delete x[0];

//CHECK#8
if (x[0] !== -1) {
  throw new Test262Error('#8: Object.prototype[0] = -1; Object.prototype.length = 1; Object.prototype.unshift = Array.prototype.unshift; x = {0:0}; x.unshift(0); delete x[0]; x[0] === -1. Actual: ' + (x[0]));
}

//CHECK#9
if (x.length !== 2) {
  throw new Test262Error('#9: Object.prototype[0] = -1; Object.prototype.length = 1; Object.prototype.unshift = Array.prototype.unshift; x = {0:0}; x.unshift(0); x.length === 1. Actual: ' + (x.length));
}

//CHECK#10
delete x.length;
if (x.length !== 1) {
  throw new Test262Error('#10: Object.prototype[1] = -1; Object.prototype.length = 1; Object.prototype.unshift = Array.prototype.unshift; x = {0:0}; x.unshift(0); delete x; x.length === 1. Actual: ' + (x.length));
}
