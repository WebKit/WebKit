// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "Step 4: If this number value is NaN, return the string \"NaN\""
es5id: 15.7.4.5_A1.3_T02
description: direct usage of NaN
---*/

//CHECK#1
if (Number.NaN.toFixed() !== "NaN") {
  throw new Test262Error('#1: Number.NaN.prototype.toFixed() === "NaN"');
}

//CHECK#2
if (Number.NaN.toFixed(0) !== "NaN") {
  throw new Test262Error('#2: Number.NaN.prototype.toFixed(0) === "NaN"');
}

//CHECK#3
if (Number.NaN.toFixed(1) !== "NaN") {
  throw new Test262Error('#3: Number.NaN.prototype.toFixed(1) === "NaN"');
}

//CHECK#4
if (Number.NaN.toFixed(1.1) !== "NaN") {
  throw new Test262Error('#4: Number.NaN.toFixed(1.1) === "NaN"');
}

//CHECK#5
if (Number.NaN.toFixed(0.9) !== "NaN") {
  throw new Test262Error('#5: Number.NaN.toFixed(0.9) === "NaN"');
}

//CHECK#6
if (Number.NaN.toFixed("1") !== "NaN") {
  throw new Test262Error('#6: Number.NaN.toFixed("1") === "NaN"');
}

//CHECK#7
if (Number.NaN.toFixed("1.1") !== "NaN") {
  throw new Test262Error('#7: Number.NaN.toFixed("1.1") === "NaN"');
}

//CHECK#8
if (Number.NaN.toFixed("0.9") !== "NaN") {
  throw new Test262Error('#8: Number.NaN.toFixed("0.9") === "NaN"');
}

//CHECK#9
if (Number.NaN.toFixed(Number.NaN) !== "NaN") {
  throw new Test262Error('#9: Number.NaN.toFixed(Number.NaN) === "NaN"');
}

//CHECK#10
if (Number.NaN.toFixed("some string") !== "NaN") {
  throw new Test262Error('#9: Number.NaN.toFixed("some string") === "NaN"');
}

//CHECK#10
try {
  s = Number.NaN.toFixed(Number.POSITIVE_INFINITY);
  throw new Test262Error('#10: Number.NaN.toFixed(Number.POSITIVE_INFINITY) should throw RangeError, not return NaN');
}
catch (e) {
  if (!(e instanceof RangeError)) {
    throw new Test262Error('#10: Number.NaN.toFixed(Number.POSITIVE_INFINITY) should throw RangeError, not ' + e);
  }
}
