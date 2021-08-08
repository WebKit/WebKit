// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array-exotic-objects-defineownproperty-p-desc
info: |
    For every integer k that is less than the value of
    the length property of A but not less than ToUint32(length),
    if A itself has a property (not an inherited property) named ToString(k),
    then delete that property
es5id: 15.4.5.1_A1.2_T1
description: Change length of array
---*/

//CHECK#1
var x = [0, , 2, , 4];
x.length = 4;
if (x[4] !== undefined) {
  throw new Test262Error('#1: x = [0,,2,,4]; x.length = 4; x[4] === undefined. Actual: ' + (x[4]));
}

//CHECK#2
x.length = 3;
if (x[3] !== undefined) {
  throw new Test262Error('#2: x = [0,,2,,4]; x.length = 4; x.length = 3; x[3] === undefined. Actual: ' + (x[3]));
}

//CHECK#3
if (x[2] !== 2) {
  throw new Test262Error('#3: x = [0,,2,,4]; x.length = 4; x.length = 3; x[2] === 2. Actual: ' + (x[2]));
}
