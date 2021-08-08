// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMonth" has { DontEnum } attributes
esid: sec-date.prototype.setmonth
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.setMonth === false) {
  throw new Test262Error('#1: The Date.prototype.setMonth property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('setMonth')) {
  throw new Test262Error('#2: The Date.prototype.setMonth property has not the attributes DontDelete');
}
