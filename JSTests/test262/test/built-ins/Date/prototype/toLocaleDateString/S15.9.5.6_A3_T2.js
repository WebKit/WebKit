// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toLocaleDateString property "length" has { ReadOnly, !
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tolocaledatestring
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.toLocaleDateString.length !== true) {
  throw new Test262Error('#1: The Date.prototype.toLocaleDateString.length property does not have the attributes DontDelete');
}

if (Date.prototype.toLocaleDateString.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.toLocaleDateString.length property does not have the attributes DontDelete');
}
