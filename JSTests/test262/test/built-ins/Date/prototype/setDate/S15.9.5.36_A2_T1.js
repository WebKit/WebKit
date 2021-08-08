// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setDate" is 1
esid: sec-date.prototype.setdate
description: The "length" property of the "setDate" is 1
---*/

if (Date.prototype.setDate.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The setDate has a "length" property');
}

if (Date.prototype.setDate.length !== 1) {
  throw new Test262Error('#2: The "length" property of the setDate is 1');
}
