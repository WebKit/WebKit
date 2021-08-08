// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCDate" is 1
esid: sec-date.prototype.setutcdate
description: The "length" property of the "setUTCDate" is 1
---*/

if (Date.prototype.setUTCDate.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The setUTCDate has a "length" property');
}

if (Date.prototype.setUTCDate.length !== 1) {
  throw new Test262Error('#2: The "length" property of the setUTCDate is 1');
}
