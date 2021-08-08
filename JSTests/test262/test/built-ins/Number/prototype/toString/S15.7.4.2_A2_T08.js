// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    toString: If radix is an integer from 2 to 36, but not 10,
    the result is a string, the choice of which is implementation-dependent
es5id: 15.7.4.2_A2_T08
description: radix is 9
---*/

//CHECK#1
if (Number.prototype.toString(9) !== "0") {
  throw new Test262Error('#1: Number.prototype.toString(9) === "0"');
}

//CHECK#2
if ((new Number()).toString(9) !== "0") {
  throw new Test262Error('#2: (new Number()).toString(9) === "0"');
}

//CHECK#3
if ((new Number(0)).toString(9) !== "0") {
  throw new Test262Error('#3: (new Number(0)).toString(9) === "0"');
}

//CHECK#4
if ((new Number(-1)).toString(9) !== "-1") {
  throw new Test262Error('#4: (new Number(-1)).toString(9) === "-1"');
}

//CHECK#5
if ((new Number(1)).toString(9) !== "1") {
  throw new Test262Error('#5: (new Number(1)).toString(9) === "1"');
}

//CHECK#6
if ((new Number(Number.NaN)).toString(9) !== "NaN") {
  throw new Test262Error('#6: (new Number(Number.NaN)).toString(9) === "NaN"');
}

//CHECK#7
if ((new Number(Number.POSITIVE_INFINITY)).toString(9) !== "Infinity") {
  throw new Test262Error('#7: (new Number(Number.POSITIVE_INFINITY)).toString(9) === "Infinity"');
}

//CHECK#8
if ((new Number(Number.NEGATIVE_INFINITY)).toString(9) !== "-Infinity") {
  throw new Test262Error('#8: (new Number(Number.NEGATIVE_INFINITY)).toString(9) === "-Infinity"');
}
