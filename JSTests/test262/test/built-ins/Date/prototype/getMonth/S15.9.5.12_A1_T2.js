// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getMonth" has { DontEnum } attributes
esid: sec-date.prototype.getmonth
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.getMonth === false) {
  throw new Test262Error('#1: The Date.prototype.getMonth property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('getMonth')) {
  throw new Test262Error('#2: The Date.prototype.getMonth property has not the attributes DontDelete');
}
