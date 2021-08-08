// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "[[Get]] from not an inherited property"
esid: sec-array.prototype.splice
description: >
    [[Prototype]] of Array instance is Array.prototype, [[Prototype]
    of Array.prototype is Object.prototype
---*/

Array.prototype[1] = -1;
var x = [0, 1];
var arr = x.splice(1, 1, 2);

//CHECK#1
if (arr.length !== 1) {
  throw new Test262Error('#1: Array.prototype[1] = -1; x = [0,1]; var arr = x.splice(1,1,2); arr.length === 1. Actual: ' + (arr.length));
}

//CHECK#2
if (arr[0] !== 1) {
  throw new Test262Error('#2: Array.prototype[1] = -1; x = [0,1]; var arr = x.splice(1,1,2); arr[0] === 1. Actual: ' + (arr[0]));
}

//CHECK#3
if (arr[1] !== -1) {
  throw new Test262Error('#3: Array.prototype[1] = -1; x = [0,1]; var arr = x.splice(1,1,2); arr[1] === -1. Actual: ' + (arr[1]));
}

//CHECK#4
if (x.length !== 2) {
  throw new Test262Error('#4: Array.prototype[1] = -1; x = [0,1]; var arr = x.splice(1,1,2); x.length === 2. Actual: ' + (x.length));
}

//CHECK#5
if (x[0] !== 0) {
  throw new Test262Error('#5: Array.prototype[1] = -1; x = [0,1]; var arr = x.splice(1,1,2); x[0] === 0. Actual: ' + (x[0]));
}

//CHECK#6
if (x[1] !== 2) {
  throw new Test262Error('#6: Array.prototype[1] = -1; x = [0,1]; var arr = x.splice(1,1,2); x[1] === 2. Actual: ' + (x[1]));
}


Object.prototype[1] = -1;
Object.prototype.length = 2;
Object.prototype.splice = Array.prototype.splice;
x = {
  0: 0,
  1: 1
};
var arr = x.splice(1, 1, 2);

//CHECK#7
if (arr.length !== 1) {
  throw new Test262Error('#7: Object.prototype[1] = -1; Object.prototype.length = 2; Object.prototype.splice = Array.prototype.splice; x = {0:0, 1:1}; var arr = x.splice(1,1,2); arr.length === 1. Actual: ' + (arr.length));
}

//CHECK#8
if (arr[0] !== 1) {
  throw new Test262Error('#8: Object.prototype[1] = -1; Object.prototype.length = 2; Object.prototype.splice = Array.prototype.splice; x = {0:0, 1:1}; var arr = x.splice(1,1,2); arr[0] === 1. Actual: ' + (arr[0]));
}

//CHECK#9
if (arr[1] !== -1) {
  throw new Test262Error('#9: Object.prototype[1] = -1; Object.prototype.length = 2; Object.prototype.splice = Array.prototype.splice; x = {0:0, 1:1}; var arr = x.splice(1,1,2); arr[1] === -1. Actual: ' + (arr[1]));
}

//CHECK#10
if (x.length !== 2) {
  throw new Test262Error('#10: Object.prototype[1] = -1; Object.prototype.length = 2; Object.prototype.splice = Array.prototype.splice; x = {0:0, 1:1}; var arr = x.splice(1,1,2); x.length === 2. Actual: ' + (x.length));
}

//CHECK#11
if (x[0] !== 0) {
  throw new Test262Error('#11: Object.prototype[1] = -1; Object.prototype.length = 2; Object.prototype.splice = Array.prototype.splice; x = {0:0, 1:1}; var arr = x.splice(1,1,2); x[0] === 0. Actual: ' + (x[0]));
}

//CHECK#12
if (x[1] !== 2) {
  throw new Test262Error('#12: Object.prototype[1] = -1; Object.prototype.length = 2; Object.prototype.splice = Array.prototype.splice; x = {0:0, 1:1}; var arr = x.splice(1,1,2); x[1] === 2. Actual: ' + (x[1]));
}
