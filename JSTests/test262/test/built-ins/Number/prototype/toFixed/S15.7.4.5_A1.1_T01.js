// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Step 1: Let f be ToInteger(fractionDigits). (If fractionDigits
    is undefined, this step produces the value 0)
es5id: 15.7.4.5_A1.1_T01
description: calling on Number prototype object
---*/

//CHECK#1
if (Number.prototype.toFixed() !== "0") {
  throw new Test262Error('#1: Number.prototype.toFixed() === "0"');
}

//CHECK#2
if (Number.prototype.toFixed(0) !== "0") {
  throw new Test262Error('#2: Number.prototype.toFixed(0) === "0"');
}

//CHECK#3
if (Number.prototype.toFixed(1) !== "0.0") {
  throw new Test262Error('#3: Number.prototype.toFixed(1) === "0.0"');
}

//CHECK#4
if (Number.prototype.toFixed(1.1) !== "0.0") {
  throw new Test262Error('#4: Number.prototype.toFixed(1.1) === "0.0"');
}

//CHECK#5
if (Number.prototype.toFixed(0.9) !== "0") {
  throw new Test262Error('#5: Number.prototype.toFixed(0.9) === "0"');
}


//CHECK#6
if (Number.prototype.toFixed("1") !== "0.0") {
  throw new Test262Error('#6: Number.prototype.toFixed("1") === "0.0"');
}

//CHECK#7
if (Number.prototype.toFixed("1.1") !== "0.0") {
  throw new Test262Error('#7: Number.prototype.toFixed("1.1") === "0.0"');
}

//CHECK#8
if (Number.prototype.toFixed("0.9") !== "0") {
  throw new Test262Error('#8: Number.prototype.toFixed("0.9") === "0"');
}

//CHECK#9
if (Number.prototype.toFixed(Number.NaN) !== "0") {
  throw new Test262Error('#9: Number.prototype.toFixed(Number.NaN) === "0"');
}

//CHECK#10
if (Number.prototype.toFixed("some string") !== "0") {
  throw new Test262Error('#9: Number.prototype.toFixed("some string") === "0"');
}

//CHECK#11
try {
  if (Number.prototype.toFixed(-0.1) !== "0") {
    throw new Test262Error('#10: Number.prototype.toFixed(-0.1) === "0"');
  }
}
catch (e) {
  throw new Test262Error('#10: Number.prototype.toFixed(-0.1) should not throw ' + e);
}
