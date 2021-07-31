// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Splice with undefined arguments
esid: sec-array.prototype.splice
description: start === undefined, end === undefined
---*/

var x = [0, 1, 2, 3];
var arr = x.splice(undefined, undefined);

//CHECK#1
arr.getClass = Object.prototype.toString;
if (arr.getClass() !== "[object " + "Array" + "]") {
  throw new Test262Error('#1: var x = [0,1,2,3]; var arr = x.splice(undefined, undefined); arr is Array object. Actual: ' + (arr.getClass()));
}

//CHECK#2
if (arr.length !== 0) {
  throw new Test262Error('#2: var x = [0,1,2,3]; var arr = x.splice(undefined, undefined); arr.length === 0. Actual: ' + (arr.length));
}

//CHECK#3
if (x.length !== 4) {
  throw new Test262Error('#3: var x = [0,1,2,3]; var arr = x.splice(undefined, undefined); x.length === 4. Actual: ' + (x.length));
}

//CHECK#4
if (x[0] !== 0) {
  throw new Test262Error('#4: var x = [0,1,2,3]; var arr = x.splice(undefined, undefined); x[0] === 0. Actual: ' + (x[0]));
}

//CHECK#5
if (x[1] !== 1) {
  throw new Test262Error('#5: var x = [0,1,2,3]; var arr = x.splice(undefined, undefined); x[1] === 1. Actual: ' + (x[1]));
}

//CHECK#6
if (x[2] !== 2) {
  throw new Test262Error('#6: var x = [0,1,2,3]; var arr = x.splice(undefined, undefined); x[2] === 2. Actual: ' + (x[2]));
}

//CHECK#7
if (x[3] !== 3) {
  throw new Test262Error('#7: var x = [0,1,2,3]; var arr = x.splice(undefined, undefined); x[3] === 3. Actual: ' + (x[3]));
}
