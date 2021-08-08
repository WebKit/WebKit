// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Step 1: Let f be ToInteger(fractionDigits). (If fractionDigits
    is undefined, this step produces the value 0)
es5id: 15.7.4.5_A1.1_T02
description: calling on Number object
---*/

//CHECK#1
if ((new Number(1)).toFixed() !== "1") {
  throw new Test262Error('#1: (new Number(1)).prototype.toFixed() === "1"');
}

//CHECK#2
if ((new Number(1)).toFixed(0) !== "1") {
  throw new Test262Error('#2: (new Number(1)).prototype.toFixed(0) === "1"');
}

//CHECK#3
if ((new Number(1)).toFixed(1) !== "1.0") {
  throw new Test262Error('#3: (new Number(1)).prototype.toFixed(1) === "1.0"');
}

//CHECK#4
if ((new Number(1)).toFixed(1.1) !== "1.0") {
  throw new Test262Error('#4: (new Number(1)).toFixed(1.1) === "1.0"');
}

//CHECK#5
if ((new Number(1)).toFixed(0.9) !== "1") {
  throw new Test262Error('#5: (new Number(1)).toFixed(0.9) === "1"');
}

//CHECK#6
if ((new Number(1)).toFixed("1") !== "1.0") {
  throw new Test262Error('#6: (new Number(1)).toFixed("1") === "1.0"');
}

//CHECK#7
if ((new Number(1)).toFixed("1.1") !== "1.0") {
  throw new Test262Error('#7: (new Number(1)).toFixed("1.1") === "1.0"');
}

//CHECK#8
if ((new Number(1)).toFixed("0.9") !== "1") {
  throw new Test262Error('#8: (new Number(1)).toFixed("0.9") === "1"');
}

//CHECK#9
if ((new Number(1)).toFixed(Number.NaN) !== "1") {
  throw new Test262Error('#9: (new Number(1)).toFixed(Number.NaN) === "1"');
}

//CHECK#10
if ((new Number(1)).toFixed("some string") !== "1") {
  throw new Test262Error('#9: (new Number(1)).toFixed("some string") === "1"');
}

//CHECK#10
try {
  if ((new Number(1)).toFixed(-0.1) !== "1") {
    throw new Test262Error('#10: (new Number(1)).toFixed(-0.1) === "1"');
  }
}
catch (e) {
  throw new Test262Error('#10: (new Number(1)).toFixed(-0.1) should not throw ' + e);
}
