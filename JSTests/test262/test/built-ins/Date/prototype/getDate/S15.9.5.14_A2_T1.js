// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getDate" is 0
esid: sec-date.prototype.getdate
description: The "length" property of the "getDate" is 0
---*/

if (Date.prototype.getDate.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The getDate has a "length" property');
}

if (Date.prototype.getDate.length !== 0) {
  throw new Test262Error('#2: The "length" property of the getDate is 0');
}
