// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toLocaleString property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tolocalestring
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.toLocaleString.length !== true) {
  throw new Test262Error('#1: The Date.prototype.toLocaleString.length property does not have the attributes DontDelete');
}

if (Date.prototype.toLocaleString.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.toLocaleString.length property does not have the attributes DontDelete');
}
