// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCMinutes" has { DontEnum } attributes
esid: sec-date.prototype.getutcminutes
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.getUTCMinutes === false) {
  throw new Test262Error('#1: The Date.prototype.getUTCMinutes property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('getUTCMinutes')) {
  throw new Test262Error('#2: The Date.prototype.getUTCMinutes property has not the attributes DontDelete');
}
