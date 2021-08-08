// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.constructor property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.constructor
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.constructor.length !== true) {
  throw new Test262Error('#1: The Date.prototype.constructor.length property does not have the attributes DontDelete');
}

if (Date.prototype.constructor.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.constructor.length property does not have the attributes DontDelete');
}
