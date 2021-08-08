// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date-year-month-date-hours-minutes-seconds-ms
info: |
    All of the arguments are optional, any arguments supplied are
    accepted but are completely ignored. A string is created and returned as
    if by the expression (new Date()).toString()
es5id: 15.9.2.1_A2
description: Use various number arguments and various types of ones
---*/

function isEqual(d1, d2) {
  if (d1 === d2) {
    return true;
  } else if (Math.abs(Date.parse(d1) - Date.parse(d2)) <= 1000) {
    return true;
  } else {
    return false;
  }
}

//CHECK#1
if (!isEqual(Date(), (new Date()).toString())) {
  throw new Test262Error('#1: Date() is equal to (new Date()).toString()');
}

//CHECK#2
if (!isEqual(Date(1), (new Date()).toString())) {
  throw new Test262Error('#2: Date(1) is equal to (new Date()).toString()');
}

//CHECK#3
if (!isEqual(Date(1970, 1), (new Date()).toString())) {
  throw new Test262Error('#3: Date(1970, 1) is equal to (new Date()).toString()');
}

//CHECK#4
if (!isEqual(Date(1970, 1, 1), (new Date()).toString())) {
  throw new Test262Error('#4: Date(1970, 1, 1) is equal to (new Date()).toString()');
}

//CHECK#5
if (!isEqual(Date(1970, 1, 1, 1), (new Date()).toString())) {
  throw new Test262Error('#5: Date(1970, 1, 1, 1) is equal to (new Date()).toString()');
}

//CHECK#6
if (!isEqual(Date(1970, 1, 1, 1), (new Date()).toString())) {
  throw new Test262Error('#7: Date(1970, 1, 1, 1) is equal to (new Date()).toString()');
}

//CHECK#8
if (!isEqual(Date(1970, 1, 1, 1, 0), (new Date()).toString())) {
  throw new Test262Error('#8: Date(1970, 1, 1, 1, 0) is equal to (new Date()).toString()');
}

//CHECK#9
if (!isEqual(Date(1970, 1, 1, 1, 0, 0), (new Date()).toString())) {
  throw new Test262Error('#9: Date(1970, 1, 1, 1, 0, 0) is equal to (new Date()).toString()');
}

//CHECK#10
if (!isEqual(Date(1970, 1, 1, 1, 0, 0, 0), (new Date()).toString())) {
  throw new Test262Error('#10: Date(1970, 1, 1, 1, 0, 0, 0) is equal to (new Date()).toString()');
}

//CHECK#11
if (!isEqual(Date(Number.NaN), (new Date()).toString())) {
  throw new Test262Error('#11: Date(Number.NaN) is equal to (new Date()).toString()');
}

//CHECK#12
if (!isEqual(Date(Number.POSITIVE_INFINITY), (new Date()).toString())) {
  throw new Test262Error('#12: Date(Number.POSITIVE_INFINITY) is equal to (new Date()).toString()');
}

//CHECK#13
if (!isEqual(Date(Number.NEGATIVE_INFINITY), (new Date()).toString())) {
  throw new Test262Error('#13: Date(Number.NEGATIVE_INFINITY) is equal to (new Date()).toString()');
}

//CHECK#14
if (!isEqual(Date(undefined), (new Date()).toString())) {
  throw new Test262Error('#14: Date(undefined) is equal to (new Date()).toString()');
}

//CHECK#15
if (!isEqual(Date(null), (new Date()).toString())) {
  throw new Test262Error('#15: Date(null) is equal to (new Date()).toString()');
}
