// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.gethours
info: The Date.prototype property "getHours" has { DontEnum } attributes
es5id: 15.9.5.18_A1_T2
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.getHours === false) {
  throw new Test262Error('#1: The Date.prototype.getHours property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('getHours')) {
  throw new Test262Error('#2: The Date.prototype.getHours property has not the attributes DontDelete');
}
