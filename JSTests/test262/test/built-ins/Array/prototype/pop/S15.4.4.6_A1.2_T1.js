// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The last element of the array is removed from the array
    and returned
esid: sec-array.prototype.pop
description: Checking this use new Array() and []
---*/

//CHECK#1
var x = new Array(0, 1, 2, 3);
var pop = x.pop();
if (pop !== 3) {
  throw new Test262Error('#1: x = new Array(0,1,2,3); x.pop() === 3. Actual: ' + (pop));
}

//CHECK#2
if (x.length !== 3) {
  throw new Test262Error('#2: x = new Array(0,1,2,3); x.pop(); x.length == 3');
}

//CHECK#3
if (x[3] !== undefined) {
  throw new Test262Error('#3: x = new Array(0,1,2,3); x.pop(); x[3] == undefined');
}

//CHECK#4
if (x[2] !== 2) {
  throw new Test262Error('#4: x = new Array(0,1,2,3); x.pop(); x[2] == 2');
}

//CHECK#5
x = [];
x[0] = 0;
x[3] = 3;
var pop = x.pop();
if (pop !== 3) {
  throw new Test262Error('#5: x = []; x[0] = 0; x[3] = 3; x.pop() === 3. Actual: ' + (pop));
}

//CHECK#6
if (x.length !== 3) {
  throw new Test262Error('#6: x = []; x[0] = 0; x[3] = 3; x.pop(); x.length == 3');
}

//CHECK#7
if (x[3] !== undefined) {
  throw new Test262Error('#7: x = []; x[0] = 0; x[3] = 3; x.pop(); x[3] == undefined');
}

//CHECK#8
if (x[2] !== undefined) {
  throw new Test262Error('#8: x = []; x[0] = 0; x[3] = 3; x.pop(); x[2] == undefined');
}

//CHECK#9
x.length = 1;
var pop = x.pop();
if (pop !== 0) {
  throw new Test262Error('#9: x = []; x[0] = 0; x[3] = 3; x.pop(); x.length = 1; x.pop() === 0. Actual: ' + (pop));
}

//CHECK#10
if (x.length !== 0) {
  throw new Test262Error('#10: x = []; x[0] = 0; x[3] = 3; x.pop(); x.length = 1; x.pop(); x.length === 0. Actual: ' + (x.length));
}
