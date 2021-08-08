// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The [[Value]] property of the newly constructed object
    is set by following steps:
    1. Call ToNumber(year)
    2. Call ToNumber(month)
    3. If date is supplied use ToNumber(date)
    4. If hours is supplied use ToNumber(hours)
    5. If minutes is supplied use ToNumber(minutes)
    6. If seconds is supplied use ToNumber(seconds)
    7. If ms is supplied use ToNumber(ms)
esid: sec-date-year-month-date-hours-minutes-seconds-ms
description: 5 arguments, (year, month, date, hours, minutes)
---*/

var myObj = function(val) {
  this.value = val;
  this.valueOf = function() {
    throw "valueOf-" + this.value;
  };
  this.toString = function() {
    throw "toString-" + this.value;
  };
};

//CHECK#1
try {
  var x1 = new Date(new myObj(1), new myObj(2), new myObj(3), new myObj(4), new myObj(5));
  throw new Test262Error("#1: The 1st step is calling ToNumber(year)");
}
catch (e) {
  if (e !== "valueOf-1") {
    throw new Test262Error("#1: The 1st step is calling ToNumber(year)");
  }
}

//CHECK#2
try {
  var x2 = new Date(1, new myObj(2), new myObj(3), new myObj(4), new myObj(5));
  throw new Test262Error("#2: The 2nd step is calling ToNumber(month)");
}
catch (e) {
  if (e !== "valueOf-2") {
    throw new Test262Error("#2: The 2nd step is calling ToNumber(month)");
  }
}

//CHECK#3
try {
  var x3 = new Date(1, 2, new myObj(3), new myObj(4), new myObj(5));
  throw new Test262Error("#3: The 3rd step is calling ToNumber(date)");
}
catch (e) {
  if (e !== "valueOf-3") {
    throw new Test262Error("#3: The 3rd step is calling ToNumber(date)");
  }
}

//CHECK#4
try {
  var x4 = new Date(1, 2, 3, new myObj(4), new myObj(5));
  throw new Test262Error("#4: The 4th step is calling ToNumber(hours)");
}
catch (e) {
  if (e !== "valueOf-4") {
    throw new Test262Error("#4: The 4th step is calling ToNumber(hours)");
  }
}

//CHECK#5
try {
  var x5 = new Date(1, 2, 3, 4, new myObj(5));
  throw new Test262Error("#5: The 5th step is calling ToNumber(minutes)");
}
catch (e) {
  if (e !== "valueOf-5") {
    throw new Test262Error("#5: The 5th step is calling ToNumber(minutes)");
  }
}
