// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMinutes" has { DontEnum } attributes
esid: sec-date.prototype.setminutes
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.setMinutes === false) {
  throw new Test262Error('#1: The Date.prototype.setMinutes property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('setMinutes')) {
  throw new Test262Error('#2: The Date.prototype.setMinutes property has not the attributes DontDelete');
}
