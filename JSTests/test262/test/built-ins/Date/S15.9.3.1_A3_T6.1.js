// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The [[Class]] property of the newly constructed object
    is set to "Date"
esid: sec-date-year-month-date-hours-minutes-seconds-ms
description: 7 arguments, (year, month, date, hours, minutes, seconds, ms)
---*/

var x1 = new Date(1899, 11, 31, 23, 59, 59, 999);
if (Object.prototype.toString.call(x1) !== "[object Date]") {
  throw new Test262Error("#1: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x2 = new Date(1899, 12, 1, 0, 0, 0, 0);
if (Object.prototype.toString.call(x2) !== "[object Date]") {
  throw new Test262Error("#2: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x3 = new Date(1900, 0, 1, 0, 0, 0, 0);
if (Object.prototype.toString.call(x3) !== "[object Date]") {
  throw new Test262Error("#3: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x4 = new Date(1969, 11, 31, 23, 59, 59, 999);
if (Object.prototype.toString.call(x4) !== "[object Date]") {
  throw new Test262Error("#4: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x5 = new Date(1969, 12, 1, 0, 0, 0, 0);
if (Object.prototype.toString.call(x5) !== "[object Date]") {
  throw new Test262Error("#5: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x6 = new Date(1970, 0, 1, 0, 0, 0, 0);
if (Object.prototype.toString.call(x6) !== "[object Date]") {
  throw new Test262Error("#6: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x7 = new Date(1999, 11, 31, 23, 59, 59, 999);
if (Object.prototype.toString.call(x7) !== "[object Date]") {
  throw new Test262Error("#7: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x8 = new Date(1999, 12, 1, 0, 0, 0, 0);
if (Object.prototype.toString.call(x8) !== "[object Date]") {
  throw new Test262Error("#8: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x9 = new Date(2000, 0, 1, 0, 0, 0, 0);
if (Object.prototype.toString.call(x9) !== "[object Date]") {
  throw new Test262Error("#9: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x10 = new Date(2099, 11, 31, 23, 59, 59, 999);
if (Object.prototype.toString.call(x10) !== "[object Date]") {
  throw new Test262Error("#10: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x11 = new Date(2099, 12, 1, 0, 0, 0, 0);
if (Object.prototype.toString.call(x11) !== "[object Date]") {
  throw new Test262Error("#11: The [[Class]] property of the newly constructed object is set to 'Date'");
}

var x12 = new Date(2100, 0, 1, 0, 0, 0, 0);
if (Object.prototype.toString.call(x12) !== "[object Date]") {
  throw new Test262Error("#12: The [[Class]] property of the newly constructed object is set to 'Date'");
}
