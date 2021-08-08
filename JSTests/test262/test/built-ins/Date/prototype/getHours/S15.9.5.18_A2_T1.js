// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.gethours
info: The "length" property of the "getHours" is 0
es5id: 15.9.5.18_A2_T1
description: The "length" property of the "getHours" is 0
---*/

if (Date.prototype.getHours.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The getHours has a "length" property');
}

if (Date.prototype.getHours.length !== 0) {
  throw new Test262Error('#2: The "length" property of the getHours is 0');
}
