// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCFullYear" is 3
esid: sec-date.prototype.setutcfullyear
description: The "length" property of the "setUTCFullYear" is 3
---*/

if (Date.prototype.setUTCFullYear.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The setUTCFullYear has a "length" property');
}

if (Date.prototype.setUTCFullYear.length !== 3) {
  throw new Test262Error('#2: The "length" property of the setUTCFullYear is 3');
}
