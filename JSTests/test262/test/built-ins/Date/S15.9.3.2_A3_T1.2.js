// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The [[Class]] property of the newly constructed object
    is set to "Date"
esid: sec-date-value
description: Test based on overwriting prototype.toString
includes: [dateConstants.js]
---*/

Date.prototype.toString = Object.prototype.toString;

var x1 = new Date(date_1899_end);
if (x1.toString() !== "[object Date]") {
  throw new Test262Error("#1: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x2 = new Date(date_1900_start);
if (x2.toString() !== "[object Date]") {
  throw new Test262Error("#2: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x3 = new Date(date_1969_end);
if (x3.toString() !== "[object Date]") {
  throw new Test262Error("#3: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x4 = new Date(date_1970_start);
if (x4.toString() !== "[object Date]") {
  throw new Test262Error("#4: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x5 = new Date(date_1999_end);
if (x5.toString() !== "[object Date]") {
  throw new Test262Error("#5: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x6 = new Date(date_2000_start);
if (x6.toString() !== "[object Date]") {
  throw new Test262Error("#6: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x7 = new Date(date_2099_end);
if (x7.toString() !== "[object Date]") {
  throw new Test262Error("#7: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x8 = new Date(date_2100_start);
if (x8.toString() !== "[object Date]") {
  throw new Test262Error("#8: The [[Class]] property of the newly constructed object is set to 'Date'");
}
