// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.gethours
info: |
    The Date.prototype.getHours property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
es5id: 15.9.5.18_A3_T2
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.getHours.length !== true) {
  throw new Test262Error('#1: The Date.prototype.getHours.length property does not have the attributes DontDelete');
}

if (Date.prototype.getHours.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.getHours.length property does not have the attributes DontDelete');
}
