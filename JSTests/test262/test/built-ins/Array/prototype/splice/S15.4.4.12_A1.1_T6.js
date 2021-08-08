// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If start is positive, use min(start, length).
    If deleteCount is positive, use min(deleteCount, length - start)
esid: sec-array.prototype.splice
description: length = deleteCount > start > 0, itemCount > 0
---*/

var x = [0, 1, 2, 3];
var arr = x.splice(1, 4, 4, 5);

//CHECK#1
arr.getClass = Object.prototype.toString;
if (arr.getClass() !== "[object " + "Array" + "]") {
  throw new Test262Error('#1: var x = [0,1,2,3]; var arr = x.splice(1,4,4,5); arr is Array object. Actual: ' + (arr.getClass()));
}

//CHECK#2
if (arr.length !== 3) {
  throw new Test262Error('#2: var x = [0,1,2,3]; var arr = x.splice(1,4,4,5); arr.length === 3. Actual: ' + (arr.length));
}

//CHECK#3
if (arr[0] !== 1) {
  throw new Test262Error('#3: var x = [0,1,2,3]; var arr = x.splice(1,4,4,5); arr[0] === 1. Actual: ' + (arr[0]));
}

//CHECK#4
if (arr[1] !== 2) {
  throw new Test262Error('#4: var x = [0,1,2,3]; var arr = x.splice(1,4,4,5); arr[1] === 2. Actual: ' + (arr[1]));
}

//CHECK#5
if (arr[2] !== 3) {
  throw new Test262Error('#5: var x = [0,1,2,3]; var arr = x.splice(1,4,4,5); arr[2] === 3. Actual: ' + (arr[2]));
}

//CHECK#6
if (x.length !== 3) {
  throw new Test262Error('#6: var x = [0,1,2,3]; var arr = x.splice(1,4,4,5); x.length === 3. Actual: ' + (x.length));
}

//CHECK#7
if (x[0] !== 0) {
  throw new Test262Error('#7: var x = [0,1,2,3]; var arr = x.splice(1,4,4,5); x[0] === 0. Actual: ' + (x[0]));
}

//CHECK#8
if (x[1] !== 4) {
  throw new Test262Error('#8: var x = [0,1,2,3]; var arr = x.splice(1,4,4,5); x[1] === 4. Actual: ' + (x[1]));
}

//CHECK#9
if (x[2] !== 5) {
  throw new Test262Error('#9: var x = [0,1,2,3]; var arr = x.splice(1,4,4,5); x[2] === 5. Actual: ' + (x[2]));
}
