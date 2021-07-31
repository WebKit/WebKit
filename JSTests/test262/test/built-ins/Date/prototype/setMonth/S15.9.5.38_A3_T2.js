// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setMonth property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setmonth
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setMonth.length !== true) {
  throw new Test262Error('#1: The Date.prototype.setMonth.length property does not have the attributes DontDelete');
}

if (Date.prototype.setMonth.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.setMonth.length property does not have the attributes DontDelete');
}
