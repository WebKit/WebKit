// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-boolean.prototype.tostring
info: |
    toString: If this boolean value is true, then the string "true"
    is returned, otherwise, this boolean value must be false, and the string
    "false" is returned
es5id: 15.6.4.2_A1_T1
description: no arguments
---*/

//CHECK#1
if (Boolean.prototype.toString() !== "false") {
  throw new Test262Error('#1: Boolean.prototype.toString() === "false"');
}

//CHECK#2
if ((new Boolean()).toString() !== "false") {
  throw new Test262Error('#2: (new Boolean()).toString() === "false"');
}

//CHECK#3
if ((new Boolean(false)).toString() !== "false") {
  throw new Test262Error('#3: (new Boolean(false)).toString() === "false"');
}

//CHECK#4
if ((new Boolean(true)).toString() !== "true") {
  throw new Test262Error('#4: (new Boolean(true)).toString() === "true"');
}

//CHECK#5
if ((new Boolean(1)).toString() !== "true") {
  throw new Test262Error('#5: (new Boolean(1)).toString() === "true"');
}

//CHECK#6
if ((new Boolean(0)).toString() !== "false") {
  throw new Test262Error('#6: (new Boolean(0)).toString() === "false"');
}

//CHECK#7
if ((new Boolean(new Object())).toString() !== "true") {
  throw new Test262Error('#7: (new Boolean(new Object())).toString() === "true"');
}
