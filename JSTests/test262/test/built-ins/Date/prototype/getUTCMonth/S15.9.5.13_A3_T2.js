// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCMonth property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcmonth
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.getUTCMonth.length !== true) {
  throw new Test262Error('#1: The Date.prototype.getUTCMonth.length property does not have the attributes DontDelete');
}

if (Date.prototype.getUTCMonth.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.getUTCMonth.length property does not have the attributes DontDelete');
}
