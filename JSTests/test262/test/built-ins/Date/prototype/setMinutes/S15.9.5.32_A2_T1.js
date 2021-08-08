// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setMinutes" is 3
esid: sec-date.prototype.setminutes
description: The "length" property of the "setMinutes" is 3
---*/

if (Date.prototype.setMinutes.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The setMinutes has a "length" property');
}

if (Date.prototype.setMinutes.length !== 3) {
  throw new Test262Error('#2: The "length" property of the setMinutes is 3');
}
