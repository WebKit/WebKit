// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setSeconds" is 2
esid: sec-date.prototype.setseconds
description: The "length" property of the "setSeconds" is 2
---*/

if (Date.prototype.setSeconds.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The setSeconds has a "length" property');
}

if (Date.prototype.setSeconds.length !== 2) {
  throw new Test262Error('#2: The "length" property of the setSeconds is 2');
}
