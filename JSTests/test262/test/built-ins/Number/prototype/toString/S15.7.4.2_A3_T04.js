// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "toString: radix should be an integer between 2 and 36"
es5id: 15.7.4.2_A3_T04
description: radix is 0
---*/

//CHECK#1
try {
  var n = Number.prototype.toString(0);
  throw new Test262Error('#1: Number.prototype.toString(0) should throw an Error');
}
catch (e) {}

//CHECK#2
try {
  var n = (new Number()).toString(0);
  throw new Test262Error('#2: (new Number()).toString(0) should throw an Error');
}
catch (e) {}

//CHECK#3
try {
  var n = (new Number(0)).toString(0);
  throw new Test262Error('#3: (new Number(0)).toString(0) should throw an Error');
}
catch (e) {}

//CHECK#4
try {
  var n = (new Number(-1)).toString(0);
  throw new Test262Error('#4: (new Number(-1)).toString(0) should throw an Error');
}
catch (e) {}

//CHECK#5
try {
  var n = (new Number(1)).toString(0);
  throw new Test262Error('#5: (new Number(1)).toString(0) should throw an Error');
}
catch (e) {}

//CHECK#6
try {
  var n = (new Number(Number.NaN)).toString(0);
  throw new Test262Error('#6: (new Number(Number.NaN)).toString(0) should throw an Error');
}
catch (e) {}

//CHECK#7
try {
  var n = (new Number(Number.POSITIVE_INFINITY)).toString(0);
  throw new Test262Error('#7: (new Number(Number.POSITIVE_INFINITY)).toString(0) should throw an Error');
}
catch (e) {}

//CHECK#8
try {
  var n = (new Number(Number.NEGATIVE_INFINITY)).toString(0);
  throw new Test262Error('#8: (new Number(Number.NEGATIVE_INFINITY)).toString(0) should throw an Error');
}
catch (e) {}
