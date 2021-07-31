// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setTime" is 1
esid: sec-date.prototype.settime
description: The "length" property of the "setTime" is 1
---*/

if (Date.prototype.setTime.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The setTime has a "length" property');
}

if (Date.prototype.setTime.length !== 1) {
  throw new Test262Error('#2: The "length" property of the setTime is 1');
}
