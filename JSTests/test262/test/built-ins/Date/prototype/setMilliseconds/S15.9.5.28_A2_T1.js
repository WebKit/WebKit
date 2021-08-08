// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setMilliseconds" is 1
esid: sec-date.prototype.setmilliseconds
description: The "length" property of the "setMilliseconds" is 1
---*/

if (Date.prototype.setMilliseconds.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The setMilliseconds has a "length" property');
}

if (Date.prototype.setMilliseconds.length !== 1) {
  throw new Test262Error('#2: The "length" property of the setMilliseconds is 1');
}
