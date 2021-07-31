// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The [[Class]] property of the newly constructed object
    is set to "Date"
esid: sec-date-value
description: Test based on delete prototype.toString
includes: [dateConstants.js]
---*/

var x1 = new Date(date_1899_end);
if (Object.prototype.toString.call(x1) !== "[object Date]") {
  throw new Test262Error("#1: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x2 = new Date(date_1900_start);
if (Object.prototype.toString.call(x2) !== "[object Date]") {
  throw new Test262Error("#2: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x3 = new Date(date_1969_end);
if (Object.prototype.toString.call(x3) !== "[object Date]") {
  throw new Test262Error("#3: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x4 = new Date(date_1970_start);
if (Object.prototype.toString.call(x4) !== "[object Date]") {
  throw new Test262Error("#4: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x5 = new Date(date_1999_end);
if (Object.prototype.toString.call(x5) !== "[object Date]") {
  throw new Test262Error("#5: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x6 = new Date(date_2000_start);
if (Object.prototype.toString.call(x6) !== "[object Date]") {
  throw new Test262Error("#6: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x7 = new Date(date_2099_end);
if (Object.prototype.toString.call(x7) !== "[object Date]") {
  throw new Test262Error("#7: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x8 = new Date(date_2100_start);
if (Object.prototype.toString.call(x8) !== "[object Date]") {
  throw new Test262Error("#8: The [[Class]] property of the newly constructed object is set to 'Date'");
}
