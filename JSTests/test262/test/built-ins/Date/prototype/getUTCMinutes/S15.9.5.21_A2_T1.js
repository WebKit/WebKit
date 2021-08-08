// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCMinutes" is 0
esid: sec-date.prototype.getutcminutes
description: The "length" property of the "getUTCMinutes" is 0
---*/

if (Date.prototype.getUTCMinutes.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The getUTCMinutes has a "length" property');
}

if (Date.prototype.getUTCMinutes.length !== 0) {
  throw new Test262Error('#2: The "length" property of the getUTCMinutes is 0');
}
