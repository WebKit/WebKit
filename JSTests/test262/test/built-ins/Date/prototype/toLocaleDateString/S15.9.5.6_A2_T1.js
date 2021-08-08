// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "toLocaleDateString" is 0
esid: sec-date.prototype.tolocaledatestring
description: The "length" property of the "toLocaleDateString" is 0
---*/

if (Date.prototype.toLocaleDateString.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The toLocaleDateString has a "length" property');
}

if (Date.prototype.toLocaleDateString.length !== 0) {
  throw new Test262Error('#2: The "length" property of the toLocaleDateString is 0');
}
