// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCMinutes" is 3
esid: sec-date.prototype.setutcminutes
description: The "length" property of the "setUTCMinutes" is 3
---*/

if (Date.prototype.setUTCMinutes.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The setUTCMinutes has a "length" property');
}

if (Date.prototype.setUTCMinutes.length !== 3) {
  throw new Test262Error('#2: The "length" property of the setUTCMinutes is 3');
}
