// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setFullYear property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setfullyear
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setFullYear.length !== true) {
  throw new Test262Error('#1: The Date.prototype.setFullYear.length property does not have the attributes DontDelete');
}

if (Date.prototype.setFullYear.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.setFullYear.length property does not have the attributes DontDelete');
}
