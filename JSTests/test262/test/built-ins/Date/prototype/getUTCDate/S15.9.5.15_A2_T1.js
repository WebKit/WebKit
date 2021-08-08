// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCDate" is 0
esid: sec-date.prototype.getutcdate
description: The "length" property of the "getUTCDate" is 0
---*/

if (Date.prototype.getUTCDate.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The getUTCDate has a "length" property');
}

if (Date.prototype.getUTCDate.length !== 0) {
  throw new Test262Error('#2: The "length" property of the getUTCDate is 0');
}
