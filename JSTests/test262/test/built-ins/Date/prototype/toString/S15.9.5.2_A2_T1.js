// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "toString" is 0
esid: sec-date.prototype.tostring
description: The "length" property of the "toString" is 0
---*/

if (Date.prototype.toString.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The toString has a "length" property');
}

if (Date.prototype.toString.length !== 0) {
  throw new Test262Error('#2: The "length" property of the toString is 0');
}
