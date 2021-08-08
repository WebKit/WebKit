// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCHours" is 4
esid: sec-date.prototype.setutchours
description: The "length" property of the "setUTCHours" is 4
---*/

if (Date.prototype.setUTCHours.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The setUTCHours has a "length" property');
}

if (Date.prototype.setUTCHours.length !== 4) {
  throw new Test262Error('#2: The "length" property of the setUTCHours is 4');
}
