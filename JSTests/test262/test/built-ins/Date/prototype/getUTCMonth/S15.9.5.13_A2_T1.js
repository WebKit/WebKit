// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCMonth" is 0
esid: sec-date.prototype.getutcmonth
description: The "length" property of the "getUTCMonth" is 0
---*/

if (Date.prototype.getUTCMonth.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The getUTCMonth has a "length" property');
}

if (Date.prototype.getUTCMonth.length !== 0) {
  throw new Test262Error('#2: The "length" property of the getUTCMonth is 0');
}
