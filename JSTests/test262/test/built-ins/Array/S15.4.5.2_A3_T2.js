// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If the length property is changed, every property whose name
    is an array index whose value is not smaller than the new length is automatically deleted
es5id: 15.4.5.2_A3_T2
description: >
    If new length greater than the name of every property whose name
    is an array index
---*/

//CHECK#1
var x = [];
x[1] = 1;
x[3] = 3;
x[5] = 5;
x.length = 4;
if (x.length !== 4) {
  throw new Test262Error('#1: x = []; x[1] = 1; x[3] = 3; x[5] = 5; x.length = 4; x.length === 4. Actual: ' + (x.length));
}

//CHECK#2
if (x[5] !== undefined) {
  throw new Test262Error('#2: x = []; x[1] = 1; x[3] = 3; x[5] = 5; x.length = 4; x[5] === undefined. Actual: ' + (x[5]));
}

//CHECK#3
if (x[3] !== 3) {
  throw new Test262Error('#3: x = []; x[1] = 1; x[3] = 3; x[5] = 5; x.length = 4; x[3] === 3. Actual: ' + (x[3]));
}

//CHECK#4
x.length = new Number(6);
if (x[5] !== undefined) {
  throw new Test262Error('#4: x = []; x[1] = 1; x[3] = 3; x[5] = 5; x.length = 4; x.length = new Number(6); x[5] === undefined. Actual: ' + (x[5]));
}

//CHECK#5
x.length = 0;
if (x[0] !== undefined) {
  throw new Test262Error('#5: x = []; x[1] = 1; x[3] = 3; x[5] = 5; x.length = 4; x.length = new Number(6); x.length = 0; x[0] === undefined. Actual: ' + (x[0]));
}

//CHECK#6
x.length = 1;
if (x[1] !== undefined) {
  throw new Test262Error('#6: x = []; x[1] = 1; x[3] = 3; x[5] = 5; x.length = 4; x.length = new Number(6); x.length = 0; x.length = 1; x[1] === undefined. Actual: ' + (x[1]));
}
