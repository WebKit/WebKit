// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCHours property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutchours
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.getUTCHours.length !== true) {
  throw new Test262Error('#1: The Date.prototype.getUTCHours.length property does not have the attributes DontDelete');
}

if (Date.prototype.getUTCHours.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.getUTCHours.length property does not have the attributes DontDelete');
}
