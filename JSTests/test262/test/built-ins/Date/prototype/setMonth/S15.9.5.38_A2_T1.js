// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setMonth" is 2
esid: sec-date.prototype.setmonth
description: The "length" property of the "setMonth" is 2
---*/

if (Date.prototype.setMonth.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The setMonth has a "length" property');
}

if (Date.prototype.setMonth.length !== 2) {
  throw new Test262Error('#2: The "length" property of the setMonth is 2');
}
