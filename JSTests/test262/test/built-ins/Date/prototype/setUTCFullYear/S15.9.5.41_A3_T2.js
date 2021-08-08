// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCFullYear property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcfullyear
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.setUTCFullYear.length !== true) {
  throw new Test262Error('#1: The Date.prototype.setUTCFullYear.length property does not have the attributes DontDelete');
}

if (Date.prototype.setUTCFullYear.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.setUTCFullYear.length property does not have the attributes DontDelete');
}
