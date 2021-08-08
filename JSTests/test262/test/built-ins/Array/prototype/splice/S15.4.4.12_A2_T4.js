// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The splice function is intentionally generic.
    It does not require that its this value be an Array object
esid: sec-array.prototype.splice
description: >
    If start is negative, use max(start + length, 0).  If deleteCount
    is positive, use min(deleteCount, length - start)
---*/

var obj = {
  0: 0,
  1: 1,
  2: 2,
  3: 3
};
obj.length = 4;
obj.splice = Array.prototype.splice;
var arr = obj.splice(-4, 3, 4, 5);

//CHECK#1
arr.getClass = Object.prototype.toString;
if (arr.getClass() !== "[object " + "Array" + "]") {
  throw new Test262Error('#1: var obj = {0:0,1:1,2:2,3:3}; obj.length = 4; obj.splice = Array.prototype.splice; var arr = obj.splice(-4,3,4,5); arr is Array object. Actual: ' + (arr.getClass()));
}

//CHECK#2
if (arr.length !== 3) {
  throw new Test262Error('#2: var obj = {0:0,1:1,2:2,3:3}; obj.length = 4; obj.splice = Array.prototype.splice; var arr = obj.splice(-4,3,4,5); arr.length === 3. Actual: ' + (arr.length));
}

//CHECK#3
if (arr[0] !== 0) {
  throw new Test262Error('#3: var obj = {0:0,1:1,2:2,3:3}; obj.length = 4; obj.splice = Array.prototype.splice; var arr = obj.splice(-4,3,4,5); arr[0] === 0. Actual: ' + (arr[0]));
}

//CHECK#4
if (arr[1] !== 1) {
  throw new Test262Error('#4: var obj = {0:0,1:1,2:2,3:3}; obj.length = 4; obj.splice = Array.prototype.splice; var arr = obj.splice(-4,3,4,5); arr[1] === 1. Actual: ' + (arr[1]));
}

//CHECK#5
if (arr[2] !== 2) {
  throw new Test262Error('#5: var obj = {0:0,1:1,2:2,3:3}; obj.length = 4; obj.splice = Array.prototype.splice; var arr = obj.splice(-4,3,4,5); arr[2] === 2. Actual: ' + (arr[2]));
}

//CHECK#6
if (obj.length !== 3) {
  throw new Test262Error('#6: var obj = {0:0,1:1,2:2,3:3}; obj.length = 4; obj.splice = Array.prototype.splice; var arr = obj.splice(-4,3,4,5); obj.length === 3. Actual: ' + (obj.length));
}

//CHECK#7
if (obj[0] !== 4) {
  throw new Test262Error('#7: var obj = {0:0,1:1,2:2,3:3}; obj.length = 4; obj.splice = Array.prototype.splice; var arr = obj.splice(-4,3,4,5); obj[0] === 4. Actual: ' + (obj[0]));
}

//CHECK#8
if (obj[1] !== 5) {
  throw new Test262Error('#8: var obj = {0:0,1:1,2:2,3:3}; obj.length = 4; obj.splice = Array.prototype.splice; var arr = obj.splice(-4,3,4,5); obj[1] === 5. Actual: ' + (obj[1]));
}

//CHECK#9
if (obj[2] !== 3) {
  throw new Test262Error('#9: var obj = {0:0,1:1,2:2,3:3}; obj.length = 4; obj.splice = Array.prototype.splice; var arr = obj.splice(-4,3,4,5); obj[2] === 3. Actual: ' + (obj[2]));
}

//CHECK#10
if (obj[3] !== undefined) {
  throw new Test262Error('#10: var obj = {0:0,1:1,2:2,3:3}; obj.length = 4; obj.splice = Array.prototype.splice; var arr = obj.splice(-4,3,4,5); obj[3] === undefined. Actual: ' + (obj[3]));
}
