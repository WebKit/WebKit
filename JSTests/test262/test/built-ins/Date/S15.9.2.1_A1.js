// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date-year-month-date-hours-minutes-seconds-ms
info: |
    When Date is called as a function rather than as a constructor,
    it should be "string" representing the current time (UTC)
es5id: 15.9.2.1_A1
description: Checking type of returned value
---*/

//CHECK#1
if (typeof Date() !== "string") {
  throw new Test262Error('#1: typeof Date() should be "string", actual is ' + (typeof Date()));
}

//CHECK#2
if (typeof Date(1) !== "string") {
  throw new Test262Error('#2: typeof Date(1) should be "string", actual is ' + (typeof Date(1)));
}

//CHECK#3
if (typeof Date(1970, 1) !== "string") {
  throw new Test262Error('#3: typeof Date(1970, 1) should be "string", actual is ' + (typeof Date(1970, 1)));
}

//CHECK#4
if (typeof Date(1970, 1, 1) !== "string") {
  throw new Test262Error('#4: typeof Date(1970, 1, 1) should be "string", actual is ' + (typeof Date(1970, 1, 1)));
}

//CHECK#5
if (typeof Date(1970, 1, 1, 1) !== "string") {
  throw new Test262Error('#5: typeof Date(1970, 1, 1, 1) should be "string", actual is ' + (typeof Date(1970, 1, 1, 1)));
}

//CHECK#6
if (typeof Date(1970, 1, 1, 1) !== "string") {
  throw new Test262Error('#7: typeof Date(1970, 1, 1, 1) should be "string", actual is ' + (typeof Date(1970, 1, 1, 1)));
}

//CHECK#8
if (typeof Date(1970, 1, 1, 1, 0) !== "string") {
  throw new Test262Error('#8: typeof Date(1970, 1, 1, 1, 0) should be "string", actual is ' + (typeof Date(1970, 1, 1, 1, 0)));
}

//CHECK#9
if (typeof Date(1970, 1, 1, 1, 0, 0) !== "string") {
  throw new Test262Error('#9: typeof Date(1970, 1, 1, 1, 0, 0) should be "string", actual is ' + (typeof Date(1970, 1, 1, 1, 0, 0)));
}

//CHECK#10
if (typeof Date(1970, 1, 1, 1, 0, 0, 0) !== "string") {
  throw new Test262Error('#10: typeof Date(1970, 1, 1, 1, 0, 0, 0) should be "string", actual is ' + (typeof Date(1970, 1, 1, 1, 0, 0, 0)));
}

//CHECK#11
if (typeof Date(Number.NaN) !== "string") {
  throw new Test262Error('#11: typeof Date(Number.NaN) should be "string", actual is ' + (typeof Date(Number.NaN)));
}

//CHECK#12
if (typeof Date(Number.POSITIVE_INFINITY) !== "string") {
  throw new Test262Error('#12: typeof Date(Number.POSITIVE_INFINITY) should be "string", actual is ' + (typeof Date(Number.POSITIVE_INFINITY)));
}

//CHECK#13
if (typeof Date(Number.NEGATIVE_INFINITY) !== "string") {
  throw new Test262Error('#13: typeof Date(Number.NEGATIVE_INFINITY) should be "string", actual is ' + (typeof Date(Number.NEGATIVE_INFINITY)));
}

//CHECK#14
if (typeof Date(undefined) !== "string") {
  throw new Test262Error('#14: typeof Date(undefined) should be "string", actual is ' + (typeof Date(undefined)));
}

//CHECK#15
if (typeof Date(null) !== "string") {
  throw new Test262Error('#15: typeof Date(null) should be "string", actual is ' + (typeof Date(null)));
}
