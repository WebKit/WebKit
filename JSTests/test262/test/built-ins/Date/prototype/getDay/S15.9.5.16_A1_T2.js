// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getday
info: The Date.prototype property "getDay" has { DontEnum } attributes
es5id: 15.9.5.16_A1_T2
description: Checking absence of DontDelete attribute
---*/

if (delete Date.prototype.getDay === false) {
  throw new Test262Error('#1: The Date.prototype.getDay property has not the attributes DontDelete');
}

if (Date.prototype.hasOwnProperty('getDay')) {
  throw new Test262Error('#2: The Date.prototype.getDay property has not the attributes DontDelete');
}
